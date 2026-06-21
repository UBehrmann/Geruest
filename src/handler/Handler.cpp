/**
 * @file Handler.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief
 */

#include "Handler.hpp"

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstring>
#include <cerrno>
#include <exception>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#ifdef __linux__
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "TextResponseCache.hpp"
#include "builders/ContentBuilder.hpp"
#include "builders/WebPConverter.hpp"
#include "GateEvaluation.hpp"
#include "data/HTTPResponse.hpp"
#include "security/Security.hpp"
#include "server/WebSocket.hpp"

namespace {

std::atomic<uint64_t> gSendfileHitCount{0};
std::atomic<uint64_t> gSendfileFallbackCount{0};

[[nodiscard]] bool isExpectedKeepAliveTeardown(const boost::system::error_code& ec, std::string_view phase) {
    if (phase != "waiting_for_request") {
        return false;
    }
    return ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
           ec == boost::asio::error::broken_pipe;
}

[[nodiscard]] const char* socketReadFailureHint(const boost::system::error_code& ec, std::string_view phase,
                                                unsigned int connectionReq) {
    if (ec == boost::asio::error::eof) {
        if (phase == "waiting_for_request" && connectionReq > 1) {
            return "client closed idle keep-alive connection (usually harmless)";
        }
        return "client closed connection (end of stream)";
    }
    if (ec == boost::asio::error::connection_reset) {
        if (phase == "waiting_for_request") {
            return "peer reset connection while waiting for next request (often browser idle teardown)";
        }
        return "connection reset by peer";
    }
    if (ec == boost::asio::error::operation_aborted) {
        return "read operation aborted (server shutdown or cancelled I/O)";
    }
    if (ec == boost::asio::error::broken_pipe) {
        return "broken pipe (peer already gone)";
    }
    if (ec == boost::asio::error::not_connected) {
        return "socket is not connected";
    }
    return nullptr;
}

}  // namespace

namespace geruest {

unsigned Handler::clientCount = 0;

Handler::Handler(boost::asio::ip::tcp::socket& socket, std::string clientIP, const ServerData& serverDataRef)
    : clientSocket(socket),
      serverData(serverDataRef),
      IP(std::move(clientIP)),
      buffer(std::make_unique<char[]>(BUFFER_SIZE)),
      fileResolver_(serverDataRef, [this](const std::string& msg) { sendToLoggerError(msg); }),
      routeDispatcher_(serverDataRef, fileResolver_) {
    serverData.incrementActiveHandlers();
}

Handler::~Handler() {
    serverData.decrementActiveHandlers();
}

void Handler::record4xxMetric() const {
    if (_countRequestInMetrics) {
        serverData.record4xx();
    }
}

void Handler::record5xxMetric() const {
    if (_countRequestInMetrics) {
        serverData.record5xx();
    }
}

void Handler::recordErrorMetric() const {
    if (_countRequestInMetrics) {
        serverData.recordError();
    }
}

boost::asio::awaitable<bool> Handler::enforcePageAccessAsync(const HTTPRequest& request,
                                                             const std::string& pagePath,
                                                             PageAccessDenyStyle denyStyle,
                                                             const std::optional<ResolvedPageGate>& resolvedGate) {
    const std::string canonPage = canonicalRequestPath(pagePath);

    if (serverData.getBasicAuth().requiresAuth(canonPage)) {
        if (!serverData.getBasicAuth().authenticate(canonPage, request.getHeader("authorization"))) {
            const std::string header = buildAuthHeader();
            if (!co_await sendSocketAsync(header.c_str(), header.size())) {
                sendToLoggerError("Failed to send auth header");
            }
            co_return false;
        }
    }

    const auto gate =
        resolvedGate.has_value() ? resolvedGate : serverData.findResolvedPageGate(pagePath);
    if (!gate.has_value()) {
        co_return true;
    }

    const bool allowed = co_await evaluateResolvedGateAsync(
        *gate, request, [this](const std::string& msg) { sendToLoggerError(msg); }, "page");
    if (allowed) {
        co_return true;
    }

    if (denyStyle == PageAccessDenyStyle::Forbidden) {
        HTTPResponse forbidden = responseForbidden(&request);
        forbidden.serializeTo(responseScratch_);
        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send merged asset gate denial for: " + pagePath);
        }
        co_return false;
    }

    HTTPResponse redirectResponse("302 Found");
    redirectResponse.setHeader("Location",
                               serverData.resolvePageGateRedirect(gate->redirectTo, request.getPathString()));
    redirectResponse.setBody("");
    redirectResponse.serializeTo(responseScratch_);
    if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
        sendToLoggerError("Failed to send page gate denial for: " + pagePath);
    }
    co_return false;
}

boost::asio::awaitable<std::optional<HTTPResponse>> Handler::checkRouteGateDenialAsync(
    const HTTPRequest& request) const {
    const auto gate = serverData.findResolvedRouteGate(request.getPathString());
    if (!gate.has_value()) {
        co_return std::nullopt;
    }

    const bool allowed = co_await evaluateResolvedGateAsync(
        *gate, request, [this](const std::string& msg) { sendToLoggerError(msg); }, "route");
    if (allowed) {
        co_return std::nullopt;
    }

    co_return responseForbidden(&request);
}

boost::asio::awaitable<bool> Handler::readSocketAsync(char* bufferToUse, size_t size, std::string_view phase) {
    if (size > INT_MAX) {
        sendToLoggerError("Size of buffer is too big.");
        co_return false;
    }

    try {
        const std::size_t n = co_await clientSocket.async_read_some(
            boost::asio::buffer(bufferToUse, size), boost::asio::use_awaitable);
        bufferLength = static_cast<std::int64_t>(n);
    } catch (const boost::system::system_error& e) {
        if (!isExpectedKeepAliveTeardown(e.code(), phase)) {
            std::ostringstream oss;
            oss << "Error reading from socket";
            if (!phase.empty()) {
                oss << " phase=" << phase;
            }
            oss << " connection_req=" << messageCount;
            oss << " socket_open=" << (clientSocket.is_open() ? "yes" : "no");
            oss << " error=[" << e.code().category().name() << ':' << e.code().value() << "] " << e.code().message();
            if (const char* hint = socketReadFailureHint(e.code(), phase, messageCount)) {
                oss << " hint=" << hint;
            }
            sendToLogger(oss.str(), LogLevel::Warning);
        }
        co_return false;
    }

    if (bufferLength == 0) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> Handler::readSocketAsync(std::string_view phase) {
    co_return co_await readSocketAsync(buffer.get(), BUFFER_SIZE, phase);
}

boost::asio::awaitable<bool> Handler::discardFromSocketAsync(size_t byteCount) {
    while (byteCount > 0) {
        const size_t chunk = std::min(byteCount, static_cast<size_t>(BUFFER_SIZE));
        if (!co_await readSocketAsync(buffer.get(), chunk, "discarding_body")) {
            co_return false;
        }
        if (bufferLength <= 0) {
            co_return false;
        }
        const size_t got = static_cast<size_t>(bufferLength);
        byteCount -= got;
    }
    co_return true;
}

boost::asio::awaitable<bool> Handler::sendSocketAsync(const char* bufferToSend, size_t size) {
    size_t startPos = 0;
    while (startPos < size) {
        const size_t chunkSize = std::min(static_cast<size_t>(BUFFER_SIZE), size - startPos);
        try {
            co_await boost::asio::async_write(clientSocket, boost::asio::buffer(bufferToSend + startPos, chunkSize),
                                              boost::asio::use_awaitable);
        } catch (const boost::system::system_error&) {
            co_return false;
        }
        startPos += chunkSize;
    }
    co_return true;
}

boost::asio::awaitable<bool> Handler::sendFileBodyZeroCopyAsync(const std::string& contentPath, size_t fileSize) {
#ifndef __linux__
    (void)contentPath;
    (void)fileSize;
    co_return false;
#else
    const int fileFd = ::open(contentPath.c_str(), O_RDONLY);
    if (fileFd < 0) {
        co_return false;
    }

    struct stat st {};
    if (::fstat(fileFd, &st) != 0 || !S_ISREG(st.st_mode)) {
        ::close(fileFd);
        co_return false;
    }

    const int socketFd = clientSocket.native_handle();
    off_t offset = 0;
    size_t remaining = fileSize;

    while (remaining > 0) {
        const size_t toSend = std::min(remaining, static_cast<size_t>(SSIZE_MAX));
        const ssize_t sent = ::sendfile(socketFd, fileFd, &offset, toSend);

        if (sent > 0) {
            remaining -= static_cast<size_t>(sent);
            continue;
        }

        if (sent == 0) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            try {
                co_await clientSocket.async_wait(boost::asio::ip::tcp::socket::wait_write, boost::asio::use_awaitable);
                continue;
            } catch (const boost::system::system_error&) {
                ::close(fileFd);
                co_return false;
            }
        }

        ::close(fileFd);
        co_return false;
    }

    ::close(fileFd);
    co_return remaining == 0;
#endif
}

void Handler::sendToLogger(const std::string& message, LogLevel level) const {
    if (serverData.shouldLog(level)) {
        std::cout << "Log: " << message << " from " << IP << std::endl;
    }
}
void Handler::sendToLoggerPages(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Info)) {
        std::cout << "Page Log: " << message << " from " << IP << std::endl;
    }
}
void Handler::sendToLoggerAPI(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Info)) {
        std::cout << "API Log: " << message << " from " << IP << std::endl;
    }
}
void Handler::sendToLoggerUser(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Info)) {
        std::cout << "User Log: " << message << " from " << IP << std::endl;
    }
}
void Handler::sendToLoggerError(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Error)) {
        std::cerr << "Error Log: " << message << " from " << IP << std::endl;
    }
}

boost::asio::awaitable<void> Handler::runAsync() {
    if (!buffer) {
        buffer = std::make_unique<char[]>(BUFFER_SIZE);
    }

    const size_t maxRequestsPerConnection = serverData.getMaxRequestsPerConnection();
    while (maxRequestsPerConnection == 0 || messageCount < maxRequestsPerConnection) {
        ++messageCount;
        std::string raw = std::move(pendingRequestData);
        pendingRequestData.clear();

        if (raw.empty()) {
            if (!co_await readSocketAsync("waiting_for_request")) {
                break;
            }
            if (bufferLength <= 0) {
                sendToLoggerError("Invalid buffer length in run loop.");
                break;
            }
            raw.assign(buffer.get(), static_cast<size_t>(bufferLength));
        }

        while (!splitHttpHeaders(raw).has_value()) {
            if (raw.size() >= kMaxHttpHeaderBytes) {
                sendToLoggerError("HTTP headers exceed maximum size.");
                co_return;
            }
            if (!co_await readSocketAsync("reading_headers")) {
                co_return;
            }
            if (bufferLength <= 0) {
                sendToLoggerError("Invalid buffer length while reading headers.");
                co_return;
            }
            raw.append(buffer.get(), static_cast<size_t>(bufferLength));
        }

        const size_t headerEnd = splitHttpHeaders(raw)->headerSectionEnd;

        bool hasCL = false;
        bool hasChunked = false;
        size_t bodyExpected = 0;
        const std::string_view headerPrefix(raw.data(), headerEnd);
        const HeaderPreflight preflight = parseHeaderPreflight(headerPrefix);

        // RFC 7231: clients may send Expect: 100-continue and wait (e.g. httpx POST with Content-Length: 0).
        if (!preflight.expect.empty() && httpExpectIs100Continue(preflight.expect)) {
            static const char k100[] = "HTTP/1.1 100 Continue\r\n\r\n";
            if (!co_await sendSocketAsync(k100, sizeof(k100) - 1)) {
                co_return;
            }
        }

        if (!preflight.contentLength.empty()) {
            if (!parseContentLengthBytes(preflight.contentLength, &bodyExpected)) {
                HTTPResponse br = responseBadRequest(nullptr);
                br.serializeTo(responseScratch_);
                co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size());
                co_return;
            }
            hasCL = true;
        }
        if (!preflight.transferEncoding.empty()) {
            hasChunked = httpConnectionHeaderHasToken(preflight.transferEncoding, "chunked");
        }

        if (hasCL && bodyExpected > kMaxHttpBodyBytes) {
            HTTPResponse br = responseBadRequest(nullptr);
            br.serializeTo(responseScratch_);
            co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size());
            const size_t already = raw.size() > headerEnd ? raw.size() - headerEnd : 0;
            const size_t remain = bodyExpected > already ? bodyExpected - already : 0;
            static_cast<void>(co_await discardFromSocketAsync(remain));
            co_return;
        }

        size_t needTotal = headerEnd + (hasCL ? bodyExpected : 0);
        if (hasChunked) {
            while (true) {
                const size_t chunkEnd = findChunkedBodyEnd(raw, headerEnd);
                if (chunkEnd != std::string::npos) {
                    needTotal = chunkEnd;
                    break;
                }
                if (!co_await readSocketAsync("reading_chunked_body")) {
                    co_return;
                }
                if (bufferLength <= 0) {
                    sendToLoggerError("Invalid buffer length when reading chunked body.");
                    co_return;
                }
                raw.append(buffer.get(), static_cast<size_t>(bufferLength));
                if (raw.size() > kMaxHttpBodyBytes + headerEnd) {
                    HTTPResponse br = responseBadRequest(nullptr);
                    br.serializeTo(responseScratch_);
                    co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size());
                    co_return;
                }
            }
        } else {
            while (raw.size() < needTotal) {
                if (!co_await readSocketAsync("reading_body")) {
                    co_return;
                }
                if (bufferLength <= 0) {
                    sendToLoggerError("Invalid buffer length when reading body.");
                    co_return;
                }
                raw.append(buffer.get(), static_cast<size_t>(bufferLength));
            }
        }

        std::shared_ptr<const std::string> messageBacking =
            std::make_shared<const std::string>(raw, 0, needTotal);
        if (raw.size() > needTotal) {
            pendingRequestData.assign(raw.data() + needTotal, raw.size() - needTotal);
        } else {
            pendingRequestData.clear();
        }
        raw.clear();

        HTTPRequest hTTPRequest(std::move(messageBacking), IP, serverData.getRoot(), serverData.getDatabaseClient());
        requestStream = std::istringstream();

        _countRequestInMetrics = !ServerData::isMetricsExcludedPath(hTTPRequest.getPathString());
        if (_countRequestInMetrics) {
            serverData.recordRequest();
        }
        {
            const auto _reqStart = std::chrono::steady_clock::now();
            co_await handleRequestAsync(&hTTPRequest);
            if (_upgraded) {
                co_return;
            }
            if (_countRequestInMetrics) {
                const auto _elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                                std::chrono::steady_clock::now() - _reqStart)
                                            .count();
                serverData.recordLatency(_elapsedUs <= 0 ? 0u
                                         : _elapsedUs > 0xFFFFFFFFLL ? 0xFFFFFFFFu
                                                                     : static_cast<uint32_t>(_elapsedUs));
            }
        }

        if (httpShouldCloseAfterResponse(hTTPRequest.getRawRequestLine(), hTTPRequest.getHeaderView("connection"))) {
            break;
        }

        memset(buffer.get(), 0, BUFFER_SIZE);
    }
    co_return;
}

boost::asio::awaitable<bool> Handler::tryHandleWebSocketAsync(HTTPRequest* request) {
    if (request == nullptr) {
        co_return false;
    }

    if (request->getMethod() != "GET") {
        co_return false;
    }

    if (!isWebSocketUpgrade(*request)) {
        if (isWebSocketUpgradeIntent(*request)
            && serverData.findMatchingWebSocketRoute(request->getPathString())) {
            HTTPResponse br = responseBadRequest(request);
            br.serializeTo(responseScratch_);
            co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size());
            _upgraded = true;
            co_return true;
        }
        co_return false;
    }

    if (!request->getBody().empty()) {
        HTTPResponse br = responseBadRequest(request);
        br.serializeTo(responseScratch_);
        co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size());
        _upgraded = true;
        co_return true;
    }

    auto wsHandler = serverData.findMatchingWebSocketRoute(request->getPathString());
    if (!wsHandler) {
        co_await sendNotFoundResponseAsync(request);
        _upgraded = true;
        co_return true;
    }

    const std::string secKey = request->getHeader("sec-websocket-key");
    const std::string acceptKey = computeAcceptKey(secKey);
    const std::string subprotocol =
        pickSubprotocol(request->getHeaderView("sec-websocket-protocol"),
                        serverData.getWebSocketSubprotocols());
    const std::string handshake = buildHandshakeResponse(acceptKey, subprotocol);
    if (!co_await sendSocketAsync(handshake.c_str(), handshake.size())) {
        _upgraded = true;
        co_return true;
    }

    WebSocketConnection ws(clientSocket, IP, subprotocol, serverData.getWebSocketLimits());
    bool handlerFailed = false;
    try {
        co_await (*wsHandler)(ws, *request);
    } catch (const std::exception& e) {
        handlerFailed = true;
        record5xxMetric();
        sendToLoggerError(std::string("Exception in WebSocket handler: ") + e.what());
    } catch (...) {
        handlerFailed = true;
        record5xxMetric();
        sendToLoggerError("Unknown exception in WebSocket handler");
    }

    if (ws.isOpen()) {
        if (handlerFailed) {
            co_await ws.close(1011, "internal error");
        } else {
            co_await ws.close(1000, "");
        }
    }

    _upgraded = true;
    co_return true;
}

boost::asio::awaitable<void> Handler::handleRequestAsync(HTTPRequest* request) {
    if (request == nullptr) {
        recordErrorMetric();
        sendToLoggerError("HTTPRequest is null.");
        std::string header = buildInternalServerErrorHeader();
        if (!co_await sendSocketAsync(header.c_str(), header.size())) {
            sendToLoggerError("Failed to send internal server error response");
        }
        co_return;
    }

    if (co_await tryHandleWebSocketAsync(request)) {
        co_return;
    }

    co_await routeDispatcher_.dispatchAsync(request, *this);
}

// TODO : Redo with HTTPResponse
/**
 * Send a response
 * @param status
 * @param contentType
 * @param content
 */
boost::asio::awaitable<void> Handler::sendResponseAsync(const std::string& status, const std::string& contentType,
                                                        const std::string& content) {
    HTTPResponse hdr(status);
    hdr.setHeader("Content-Type", contentType);
    hdr.setHeader("Content-Length", std::to_string(content.size()));
    hdr.serializeTo(responseScratch_);
    responseScratch_ += content;

    if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
        sendToLoggerError("Failed to send response: " + status);
    }
    co_return;
}

boost::asio::awaitable<void> Handler::sendNotFoundResponseAsync(HTTPRequest* httpRequest) {
    record4xxMetric();
    if (httpRequest != nullptr) {
        sendToLoggerError("404 route miss. path=" + httpRequest->getPathString() +
                          " request_line=" + httpRequest->getRawRequestLine());
    } else {
        sendToLoggerError("404 route miss. null request context");
    }
    if (serverData.hasNotFoundPage() && httpRequest != nullptr) {
        std::string notFoundPath = serverData.getNotFoundPage();
        if (!notFoundPath.empty() && notFoundPath[0] != '/') {
            notFoundPath = "/" + notFoundPath;
        }

        // If the original request had a language prefix in the URL (e.g. /de/missing),
        // serve the language-specific 404 page (e.g. /de/404.html → root/html/de/404.html).
        if (serverData.hasLanguages()) {
            notFoundPath = serverData.localizePathWithRequestLanguage(notFoundPath, httpRequest->getPathString());
        }

        const std::string extension = StaticFileResolver::getExtension(notFoundPath);
        const std::string contentType = StaticFileResolver::getContentType(extension);
        std::string resolvedNotFoundPath = notFoundPath;

        // Support both framework-relative paths (e.g. /404.html) and absolute filesystem paths.
        if (!(std::filesystem::path(resolvedNotFoundPath).is_absolute() && std::filesystem::exists(resolvedNotFoundPath))) {
            resolvedNotFoundPath = fileResolver_.buildPath(notFoundPath, extension, *httpRequest);
        }

        if (!resolvedNotFoundPath.empty()) {
            if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
                std::unique_ptr<ContentBuilder> contentBuilder =
                    ContentBuilder::create(contentType, resolvedNotFoundPath, serverData);

                if (contentBuilder && contentBuilder->size() > 0) {
                    co_await sendResponseAsync("404 Not Found", contentType, contentBuilder->file());
                    co_return;
                }
            } else {
                std::ifstream file(resolvedNotFoundPath, std::ios::binary);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    co_await sendResponseAsync("404 Not Found", contentType, content);
                    co_return;
                }
            }
        }

        sendToLoggerError("Configured 404 page could not be loaded: " + serverData.getNotFoundPage());
    }

    co_await sendResponseAsync("404 Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
    co_return;
}

boost::asio::awaitable<void> Handler::sendServiceUnavailableResponseAsync(const std::string& why) {
    record5xxMetric();
    serverData.recordFileOpenFailure();
    sendToLoggerError("Service unavailable while serving file: " + why);
    co_await sendResponseAsync("503 Service Unavailable", "text/plain",
                               "Service temporarily unavailable. Please retry.\n");
    co_return;
}

/**
 * Send a file
 * @param contentType
 * @param contentPath
 */
boost::asio::awaitable<void> Handler::sendFileAsync(const std::string& contentType, const std::string& contentPath,
                                                    HTTPRequest* httpRequest) {
    char bufferToSend[BUFFER_SIZE];

    if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
        const std::string cacheKey = TextResponseCache::makeKey(contentType, contentPath);
        const std::string requestPath = httpRequest != nullptr ? httpRequest->getPathString() : std::string();
        std::optional<std::string> mergedAssetOwnerPage;
        if (httpRequest != nullptr &&
            (contentType == "text/javascript" || contentType == "text/css")) {
            mergedAssetOwnerPage = serverData.findMergedAssetOwnerPagePath(requestPath);
        }

        const std::optional<ResolvedPageGate> resolvedPageGate =
            contentType == "text/html" && httpRequest != nullptr
                ? serverData.findResolvedPageGate(requestPath)
                : std::nullopt;
        const bool perRequestText =
            httpRequest != nullptr &&
            ((contentType == "text/html" &&
              (serverData.getBasicAuth().requiresAuth(requestPath) || resolvedPageGate.has_value())) ||
             (mergedAssetOwnerPage.has_value() &&
              serverData.pageRequiresAccessControl(*mergedAssetOwnerPage)));

        if (perRequestText && httpRequest) {
            const std::string& accessPath =
                contentType == "text/html" ? requestPath : *mergedAssetOwnerPage;
            const PageAccessDenyStyle denyStyle = contentType == "text/html"
                                                      ? PageAccessDenyStyle::Redirect
                                                      : PageAccessDenyStyle::Forbidden;
            if (!co_await enforcePageAccessAsync(*httpRequest, accessPath, denyStyle, resolvedPageGate)) {
                co_return;
            }
        }

        if (!perRequestText) {
            if (const std::shared_ptr<const std::string> cached = TextResponseCache::instance().lookup(
                    cacheKey, serverData.isDevMode(), serverData.getTextResponseCacheMaxEntryBytes(),
                    serverData.getTextResponseCacheMaxTotalBytes())) {
                if (!co_await sendSocketAsync(cached->data(), cached->size())) {
                    sendToLoggerError("Failed to send cached file: " + contentPath);
                }
                co_return;
            }
        }

        std::unique_ptr<ContentBuilder> contentBuilder = ContentBuilder::create(contentType, contentPath, serverData);

        if (!contentBuilder) {
            co_await sendNotFoundResponseAsync(httpRequest);
            co_return;
        }

        if (contentBuilder->size() == 0) {
            if (!contentPath.empty() && std::filesystem::exists(contentPath)) {
                co_await sendServiceUnavailableResponseAsync(contentPath);
            } else {
                co_await sendNotFoundResponseAsync(httpRequest);
            }
            co_return;
        }

        HTTPResponse htmlResponse("200 OK");

        htmlResponse.setHeader("Content-Type", contentType);

        htmlResponse.setBody(contentBuilder->file());

        htmlResponse.serializeTo(responseScratch_);
        if (!perRequestText) {
            TextResponseCache::instance().store(cacheKey, contentPath, responseScratch_, serverData.isDevMode(),
                                                serverData.getTextResponseCacheMaxEntryBytes(),
                                                serverData.getTextResponseCacheMaxTotalBytes());
        }

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send file: " + contentPath);
        }

    } else {
        if (contentType == "image/webp" && serverData.isDevMode() && serverData.getWebPConversion()) {
            auto cachedWebP = serverData.devAssetCache().getWebP(contentPath);
            if (cachedWebP && !cachedWebP->empty()) {
                HTTPResponse htmlResponse("200 OK");
                htmlResponse.setHeader("Content-Type", contentType);
                htmlResponse.setHeader("Content-Length", std::to_string(cachedWebP->size()));

                htmlResponse.serializeTo(responseScratch_);

                if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                    sendToLoggerError("Failed to send cached WebP header: " + contentPath);
                    co_return;
                }

                if (!co_await sendSocketAsync(reinterpret_cast<const char*>(cachedWebP->data()), cachedWebP->size())) {
                    sendToLoggerError("Failed to send cached WebP data: " + contentPath);
                }
                co_return;
            }
        }

        std::error_code fsErr;
        const uintmax_t fileSizeRaw = std::filesystem::file_size(contentPath, fsErr);
        if (!fsErr && fileSizeRaw <= static_cast<uintmax_t>(SIZE_MAX)) {
            const size_t fileSize = static_cast<size_t>(fileSizeRaw);

            HTTPResponse htmlResponse("200 OK");
            htmlResponse.setHeader("Content-Type", contentType);
            htmlResponse.setHeader("Content-Length", std::to_string(fileSize));
            htmlResponse.serializeTo(responseScratch_);

            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                sendToLoggerError("Failed to send binary file header: " + contentPath);
                co_return;
            }

            if (co_await sendFileBodyZeroCopyAsync(contentPath, fileSize)) {
                ++gSendfileHitCount;
                co_return;
            }

            ++gSendfileFallbackCount;
            sendToLoggerError("Zero-copy sendfile failed after header write: " + contentPath);
            co_return;
        }

        std::ifstream file(contentPath, std::ios::binary);

        if (!file.is_open()) {
            if (contentType == "image/webp" && serverData.getWebPConversion()) {
                std::string basePath = contentPath;
                size_t      dotPos = basePath.find_last_of('.');
                if (dotPos != std::string::npos) {
                    basePath = basePath.substr(0, dotPos);
                }

                std::string              sourcePath;
                std::vector<std::string> extensions = {".jpg", ".jpeg", ".png"};

                for (const auto& ext : extensions) {
                    if (std::filesystem::exists(basePath + ext)) {
                        sourcePath = basePath + ext;
                        break;
                    }
                }

                if (!sourcePath.empty()) {
                    bool cacheOnly = serverData.isDevMode();
                    if (WebPConverter::convertImage(sourcePath, contentPath, cacheOnly, serverData.getWebPQuality())) {
                        if (cacheOnly) {
                            auto webpData = serverData.devAssetCache().getWebP(contentPath);
                            if (webpData && !webpData->empty()) {
                                HTTPResponse webpResponse("200 OK");
                                webpResponse.setHeader("Content-Type", contentType);
                                webpResponse.setHeader("Content-Length", std::to_string(webpData->size()));

                                webpResponse.serializeTo(responseScratch_);

                                if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                                    sendToLoggerError("Failed to send on-demand WebP header: " + contentPath);
                                    co_return;
                                }

                                if (!co_await sendSocketAsync(reinterpret_cast<const char*>(webpData->data()),
                                                              webpData->size())) {
                                    sendToLoggerError("Failed to send on-demand WebP data: " + contentPath);
                                }
                                co_return;
                            }
                        } else {
                            file.open(contentPath, std::ios::binary);
                        }
                    } else {
                        sendToLoggerError("WebP conversion failed for " + sourcePath
                                          + ", serving original format instead");
                        std::ifstream origFile(sourcePath, std::ios::binary);
                        if (origFile.is_open()) {
                            origFile.seekg(0, std::ios::end);
                            size_t origSize = static_cast<size_t>(origFile.tellg());
                            origFile.seekg(0, std::ios::beg);

                            const std::string origType =
                                StaticFileResolver::getContentType(StaticFileResolver::getExtension(sourcePath));
                            HTTPResponse      origResp("200 OK");
                            origResp.setHeader("Content-Type", origType);
                            origResp.setHeader("Content-Length", std::to_string(origSize));
                            origResp.serializeTo(responseScratch_);

                            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                                sendToLoggerError("Failed to send fallback image header: " + sourcePath);
                                co_return;
                            }
                            char fallbackBuf[BUFFER_SIZE];
                            while (!origFile.eof()) {
                                origFile.read(fallbackBuf, BUFFER_SIZE);
                                if (!co_await sendSocketAsync(fallbackBuf, static_cast<size_t>(origFile.gcount()))) {
                                    sendToLoggerError("Failed to send fallback image data: " + sourcePath);
                                    break;
                                }
                            }
                        } else if (std::filesystem::exists(sourcePath)) {
                            co_await sendServiceUnavailableResponseAsync(sourcePath);
                        }
                        co_return;
                    }
                }
            }

            if (!file.is_open()) {
                if (!contentPath.empty() && std::filesystem::exists(contentPath)) {
                    co_await sendServiceUnavailableResponseAsync(contentPath);
                } else {
                    sendToLogger("File not found: " + contentPath);
                    co_await sendNotFoundResponseAsync(httpRequest);
                }
                co_return;
            }
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        HTTPResponse htmlResponse("200 OK");

        htmlResponse.setHeader("Content-Type", contentType);
        htmlResponse.setHeader("Content-Length", std::to_string(fileSize));

        htmlResponse.serializeTo(responseScratch_);

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send binary file header: " + contentPath);
            file.close();
            co_return;
        }

        while (!file.eof()) {
            file.read(bufferToSend, BUFFER_SIZE);
            if (!co_await sendSocketAsync(bufferToSend, static_cast<size_t>(file.gcount()))) {
                sendToLoggerError("Failed to send binary file chunk: " + contentPath);
                break;
            }
        }

        file.close();
    }
    co_return;
}

}  // namespace geruest
