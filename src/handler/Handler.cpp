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
#include <charconv>
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
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#ifdef __linux__
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "builders/AssetMerger.hpp"
#include "builders/CSSBuilder.hpp"
#include "builders/ContentBuilder.hpp"
#include "builders/HTMLBuilder.hpp"
#include "builders/JSBuilder.hpp"
#include "builders/WebPConverter.hpp"
#include "data/HTTPResponse.hpp"
#include "data/MethodNotAllowed.hpp"
#include "security/Security.hpp"
#include "server/WebSocket.hpp"

namespace {

constexpr size_t kMaxHttpHeaderBytes = 65536;
constexpr size_t kMaxHttpBodyBytes = 16 * 1024 * 1024;

struct TextResponseCacheEntry {
    std::shared_ptr<const std::string> payload;
    std::filesystem::file_time_type mtime{};
    bool hasMtime = false;
    size_t sizeBytes = 0;
};

std::mutex gTextResponseCacheMutex;
std::unordered_map<std::string, TextResponseCacheEntry> gTextResponseCache;
size_t gTextResponseCacheBytes = 0;
std::atomic<uint64_t> gSendfileHitCount{0};
std::atomic<uint64_t> gSendfileFallbackCount{0};

[[nodiscard]] std::string textResponseCacheKey(const std::string& contentType, const std::string& contentPath) {
    return contentType + "|" + contentPath;
}

[[nodiscard]] std::shared_ptr<const std::string> lookupTextResponseCache(const std::string& key,
                                                                          bool devMode,
                                                                          size_t maxEntryBytes,
                                                                          size_t maxTotalBytes) {
    if (devMode || maxEntryBytes == 0 || maxTotalBytes == 0) {
        return {};
    }
    std::lock_guard<std::mutex> lock(gTextResponseCacheMutex);
    auto it = gTextResponseCache.find(key);
    if (it == gTextResponseCache.end()) {
        return {};
    }
    return it->second.payload;
}

void storeTextResponseCache(const std::string& key, const std::string& contentPath, const std::string& payload,
                            bool devMode, size_t maxEntryBytes, size_t maxTotalBytes) {
    if (devMode || payload.empty() || maxEntryBytes == 0 || maxTotalBytes == 0 || payload.size() > maxEntryBytes) {
        return;
    }

    TextResponseCacheEntry entry;
    entry.payload = std::make_shared<const std::string>(payload);
    entry.sizeBytes = payload.size();
    std::error_code ec;
    entry.mtime = std::filesystem::last_write_time(contentPath, ec);
    entry.hasMtime = !ec;

    std::lock_guard<std::mutex> lock(gTextResponseCacheMutex);
    auto existing = gTextResponseCache.find(key);
    if (existing != gTextResponseCache.end()) {
        gTextResponseCacheBytes -= existing->second.sizeBytes;
        gTextResponseCache.erase(existing);
    }
    while (gTextResponseCacheBytes + entry.sizeBytes > maxTotalBytes && !gTextResponseCache.empty()) {
        auto victim = gTextResponseCache.begin();
        gTextResponseCacheBytes -= victim->second.sizeBytes;
        gTextResponseCache.erase(victim);
    }
    gTextResponseCacheBytes += entry.sizeBytes;
    gTextResponseCache.emplace(key, std::move(entry));
}

// Same delimiter precedence as HTTPRequest::parseHeadersAndBody ("\\r\\n\\r\\n", "\\n\\n", "\\r\\r").
size_t findHeaderEndPos(const std::string& raw) {
    size_t p = raw.find("\r\n\r\n");
    if (p != std::string::npos) {
        return p + 4;
    }
    p = raw.find("\n\n");
    if (p != std::string::npos) {
        return p + 2;
    }
    p = raw.find("\r\r");
    if (p != std::string::npos) {
        return p + 2;
    }
    return std::string::npos;
}

[[nodiscard]] inline unsigned char asciiLower(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] bool iequalsAsciiNoSpace(std::string_view a, std::string_view b) {
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size()) {
        while (i < a.size() && std::isspace(static_cast<unsigned char>(a[i]))) ++i;
        while (j < b.size() && std::isspace(static_cast<unsigned char>(b[j]))) ++j;
        if (i >= a.size() || j >= b.size()) break;
        if (asciiLower(static_cast<unsigned char>(a[i])) != asciiLower(static_cast<unsigned char>(b[j]))) {
            return false;
        }
        ++i;
        ++j;
    }
    while (i < a.size() && std::isspace(static_cast<unsigned char>(a[i]))) ++i;
    while (j < b.size() && std::isspace(static_cast<unsigned char>(b[j]))) ++j;
    return i == a.size() && j == b.size();
}

[[nodiscard]] std::string_view trimSv(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

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

struct HeaderPreflight {
    std::string_view expect;
    std::string_view contentLength;
    std::string_view transferEncoding;
};

[[nodiscard]] HeaderPreflight parseHeaderPreflight(std::string_view headerPrefix) {
    HeaderPreflight out{};
    const size_t firstNl = headerPrefix.find('\n');
    if (firstNl == std::string_view::npos) {
        return out;
    }
    std::string_view rest = headerPrefix.substr(firstNl + 1);
    while (!rest.empty()) {
        const size_t lineEnd = rest.find('\n');
        std::string_view line = rest.substr(0, lineEnd);
        if (lineEnd == std::string_view::npos) {
            rest = {};
        } else {
            rest.remove_prefix(lineEnd + 1);
        }
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        const std::string_view key = line.substr(0, colon);
        const std::string_view value = trimSv(line.substr(colon + 1));
        if (out.expect.empty() && iequalsAsciiNoSpace(key, "expect")) {
            out.expect = value;
        } else if (out.contentLength.empty() && iequalsAsciiNoSpace(key, "content-length")) {
            out.contentLength = value;
        } else if (out.transferEncoding.empty() && iequalsAsciiNoSpace(key, "transfer-encoding")) {
            out.transferEncoding = value;
        }
        if (!out.expect.empty() && !out.contentLength.empty() && !out.transferEncoding.empty()) {
            break;
        }
    }
    return out;
}

[[nodiscard]] bool containsChunkedToken(std::string_view v) {
    size_t i = 0;
    while (i < v.size()) {
        while (i < v.size() && (v[i] == ' ' || v[i] == '\t' || v[i] == ',')) ++i;
        size_t j = i;
        while (j < v.size() && v[j] != ',') ++j;
        std::string_view token = trimSv(v.substr(i, j - i));
        if (iequalsAsciiNoSpace(token, "chunked")) {
            return true;
        }
        i = j;
    }
    return false;
}

[[nodiscard]] size_t findChunkedBodyEnd(const std::string& raw, size_t bodyStart) {
    size_t pos = bodyStart;
    while (true) {
        const size_t lineEnd = raw.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            return std::string::npos;
        }

        std::string_view sizeLine(raw.data() + pos, lineEnd - pos);
        const size_t semi = sizeLine.find(';');
        if (semi != std::string_view::npos) {
            sizeLine = sizeLine.substr(0, semi);
        }
        sizeLine = trimSv(sizeLine);
        if (sizeLine.empty()) {
            return std::string::npos;
        }

        unsigned long long chunkSize = 0;
        const char* begin = sizeLine.data();
        const char* end = sizeLine.data() + sizeLine.size();
        const std::from_chars_result parsed = std::from_chars(begin, end, chunkSize, 16);
        if (parsed.ec != std::errc() || parsed.ptr != end) {
            return std::string::npos;
        }

        pos = lineEnd + 2;
        if (chunkSize == 0) {
            // No trailers case: "0\\r\\n\\r\\n" => immediate CRLF terminator.
            if (pos + 2 <= raw.size() && raw.compare(pos, 2, "\r\n") == 0) {
                return pos + 2;
            }
            const size_t trailerEnd = raw.find("\r\n\r\n", pos);
            if (trailerEnd == std::string::npos) {
                return std::string::npos;
            }
            return trailerEnd + 4;
        }

        const size_t chunkDataEnd = pos + static_cast<size_t>(chunkSize);
        if (chunkDataEnd + 2 > raw.size()) {
            return std::string::npos;
        }
        if (raw.compare(chunkDataEnd, 2, "\r\n") != 0) {
            return std::string::npos;
        }
        pos = chunkDataEnd + 2;
    }
}

bool parseContentLengthBytes(std::string_view cl, size_t* out) {
    if (cl.empty()) {
        return false;
    }
    unsigned long long v = 0;
    const char* begin = cl.data();
    const char* end = cl.data() + cl.size();
    const std::from_chars_result result = std::from_chars(begin, end, v);
    if (result.ec != std::errc() || result.ptr != end) {
        return false;
    }
    *out = static_cast<size_t>(v);
    return true;
}

/** Extension -> site-relative root (without html/htm; those use htmlMount in buildPath). */
const std::unordered_map<std::string, std::string>& handlerAssetRootByExtension() {
    static const std::unordered_map<std::string, std::string> m = {
        {"css", "/assets/css"},
        {"js", "/assets/js"},
        {"jpg", "/assets/images"},
        {"jpeg", "/assets/images"},
        {"png", "/assets/images"},
        {"gif", "/assets/images"},
        {"svg", "/assets/images"},
        {"ico", "/assets/images"},
        {"webp", "/assets/images"},
        {"JSON", "/assets/JSONs"},
        {"pdf", "/assets/docs"},
        {"zip", "/assets/docs"},
        {"mp3", "/assets/audio"},
        {"mp4", "/assets/video"},
        {"xml", "/assets/docs"},
        {"csv", "/assets/docs"},
        {"txt", "/assets/docs"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& handlerContentTypeByExtension() {
    static const std::unordered_map<std::string, std::string> m = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "text/javascript"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"JSON", "application/JSON"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"xml", "application/xml"},
        {"csv", "text/csv"},
        {"txt", "text/plain"},
    };
    return m;
}

}  // namespace

namespace geruest {

unsigned Handler::clientCount = 0;

Handler::Handler(boost::asio::ip::tcp::socket& socket, std::string clientIP, const ServerData& serverDataRef)
    : clientSocket(socket), serverData(serverDataRef), IP(std::move(clientIP)), buffer(std::make_unique<char[]>(BUFFER_SIZE)) {
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

boost::asio::awaitable<std::optional<HTTPResponse>> Handler::checkPageGateDenialAsync(
    const HTTPRequest& request) const {
    const auto gate = serverData.findResolvedPageGate(request.getPathString());
    if (!gate.has_value()) {
        co_return std::nullopt;
    }

    bool allowed = false;
    try {
        if (gate->async) {
            allowed = co_await gate->asyncHandler(request);
        } else {
            allowed = gate->syncHandler(request);
        }
    } catch (const std::exception& e) {
        sendToLoggerError(std::string("Exception in page gate handler: ") + e.what());
    } catch (...) {
        sendToLoggerError("Unknown exception in page gate handler");
    }

    if (allowed) {
        co_return std::nullopt;
    }

    HTTPResponse redirectResponse("302 Found");
    redirectResponse.setHeader("Location",
                               serverData.resolvePageGateRedirect(gate->redirectTo, request.getPathString()));
    redirectResponse.setBody("");
    co_return redirectResponse;
}

std::optional<HTTPResponse> Handler::checkRouteGateDenial(const HTTPRequest& request) const {
    const auto gate = serverData.findMatchingRouteGate(request.getPathString());
    if (!gate.has_value()) {
        return std::nullopt;
    }

    bool allowed = false;
    try {
        allowed = gate->handler(request);
    } catch (const std::exception& e) {
        sendToLoggerError(std::string("Exception in route gate handler: ") + e.what());
    } catch (...) {
        sendToLoggerError("Unknown exception in route gate handler");
    }

    if (allowed) {
        return std::nullopt;
    }

    return responseForbidden(&request);
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

        while (findHeaderEndPos(raw) == std::string::npos) {
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

        const size_t headerEnd = findHeaderEndPos(raw);

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
            hasChunked = containsChunkedToken(preflight.transferEncoding);
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

    // Priority rule 1+2: redirects first (exact redirect, then wildcard redirect)
    auto redirectMatch = serverData.findMatchingRedirect(request->getPathString());
    if (redirectMatch.has_value()) {
        const std::string& target = redirectMatch->first;
        const int statusCode = redirectMatch->second;
        const std::string statusText = (statusCode == 302) ? "302 Found" : "301 Moved Permanently";

        HTTPResponse redirectResponse(statusText);
        redirectResponse.setHeader("Location", target);
        redirectResponse.setBody("");

        redirectResponse.serializeTo(responseScratch_);
        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send redirect response for: " + request->getPathString());
        }
        co_return;
    }

    std::string path = request->getPathString();

    // Priority rule 3+4: normal sync routes (exact route, then wildcard route)
    auto routeHandler = serverData.findMatchingRoute(path);
    if (routeHandler) {
        if (auto denial = checkRouteGateDenial(*request)) {
            record4xxMetric();
            denial->serializeTo(responseScratch_);
            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                sendToLoggerError("Failed to send route gate denial for: " + path);
            }
            co_return;
        }

        // co_await is not allowed inside catch clauses; build the body first, send once.
        const char* failLog = nullptr;
        try {
            HTTPResponse response = (*routeHandler)(*request);

            const std::string& _st = response.getStatus();
            if (!_st.empty()) {
                if (_st[0] == '4') {
                    record4xxMetric();
                } else if (_st[0] == '5') {
                    record5xxMetric();
                }
            }

            response.serializeTo(responseScratch_);
            failLog = "Failed to send route response for: ";
        } catch (const method_not_allowed& e) {
            HTTPResponse response = responseMethodNotAllowed(request, e.allowMethods());
            record4xxMetric();
            response.serializeTo(responseScratch_);
            failLog = "Failed to send 405 for: ";
        } catch (const std::exception& e) {
            sendToLoggerError(std::string("Exception in route handler: ") + e.what());
            HTTPResponse response = responseInternalServerError(request);
            record5xxMetric();
            response.serializeTo(responseScratch_);
            failLog = "Failed to send 500 for: ";
        } catch (...) {
            sendToLoggerError("Unknown exception in route handler");
            HTTPResponse response = responseInternalServerError(request);
            record5xxMetric();
            response.serializeTo(responseScratch_);
            failLog = "Failed to send 500 for: ";
        }

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError(std::string(failLog) + request->getPathString());
        }

        co_return;
    }

    // Priority rule 5+6: async routes (exact route, then wildcard route)
    auto asyncRouteHandler = serverData.findMatchingAsyncRoute(path);
    if (asyncRouteHandler) {
        if (auto denial = checkRouteGateDenial(*request)) {
            record4xxMetric();
            denial->serializeTo(responseScratch_);
            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                sendToLoggerError("Failed to send route gate denial for: " + path);
            }
            co_return;
        }

        const char* failLog = nullptr;
        try {
            HTTPResponse response = co_await (*asyncRouteHandler)(*request);

            const std::string& _st = response.getStatus();
            if (!_st.empty()) {
                if (_st[0] == '4') {
                    record4xxMetric();
                } else if (_st[0] == '5') {
                    record5xxMetric();
                }
            }

            response.serializeTo(responseScratch_);
            failLog = "Failed to send async route response for: ";
        } catch (const method_not_allowed& e) {
            HTTPResponse response = responseMethodNotAllowed(request, e.allowMethods());
            record4xxMetric();
            response.serializeTo(responseScratch_);
            failLog = "Failed to send 405 for: ";
        } catch (const std::exception& e) {
            sendToLoggerError(std::string("Exception in async route handler: ") + e.what());
            HTTPResponse response = responseInternalServerError(request);
            record5xxMetric();
            response.serializeTo(responseScratch_);
            failLog = "Failed to send 500 for: ";
        } catch (...) {
            sendToLoggerError("Unknown exception in async route handler");
            HTTPResponse response = responseInternalServerError(request);
            record5xxMetric();
            response.serializeTo(responseScratch_);
            failLog = "Failed to send 500 for: ";
        }

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError(std::string(failLog) + request->getPathString());
        }

        co_return;
    }

    if (path.rfind("/api/", 0) == 0) {
        sendToLoggerError("No API route matched. path=" + path + " request_line=" + request->getRawRequestLine());
    }

    // Here html logic should be added
    std::string extension = getExtension(path);
    std::string content_type = getContentType(extension);
    std::string contentPath = buildPath(path, extension, request);

    co_await sendFileAsync(content_type, contentPath, request);
    co_return;
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
            const std::string& reqPath = httpRequest->getPathString();
            if (reqPath.size() >= 4 && reqPath[0] == '/' &&
                std::isalpha(static_cast<unsigned char>(reqPath[1])) &&
                std::isalpha(static_cast<unsigned char>(reqPath[2])) &&
                reqPath[3] == '/') {
                const std::string langCode = reqPath.substr(1, 2);
                for (const auto& lang : serverData.getAvailableLanguages()) {
                    if (lang == langCode) {
                        notFoundPath = "/" + langCode + notFoundPath;
                        break;
                    }
                }
            }
        }

        const std::string extension = getExtension(notFoundPath);
        const std::string contentType = getContentType(extension);
        std::string resolvedNotFoundPath = notFoundPath;

        // Support both framework-relative paths (e.g. /404.html) and absolute filesystem paths.
        if (!(std::filesystem::path(resolvedNotFoundPath).is_absolute() && std::filesystem::exists(resolvedNotFoundPath))) {
            resolvedNotFoundPath = buildPath(notFoundPath, extension, httpRequest);
        }

        if (!resolvedNotFoundPath.empty()) {
            if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
                std::unique_ptr<ContentBuilder> contentBuilder;

                if (contentType == "text/html") {
                    contentBuilder = std::make_unique<HtmlBuilder>(resolvedNotFoundPath, serverData);
                } else if (contentType == "text/javascript") {
                    contentBuilder = std::make_unique<JSBuilder>(resolvedNotFoundPath, serverData);
                } else if (contentType == "text/css") {
                    contentBuilder = std::make_unique<CSSBuilder>(resolvedNotFoundPath, serverData);
                }

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
        const std::string cacheKey = textResponseCacheKey(contentType, contentPath);
        const std::string requestPath = httpRequest != nullptr ? httpRequest->getPathString() : std::string();
        const bool perRequestHtml = contentType == "text/html" && httpRequest != nullptr &&
                                    (serverData.getBasicAuth().requiresAuth(requestPath) ||
                                     serverData.findResolvedPageGate(requestPath).has_value());

        if (!perRequestHtml) {
            if (const std::shared_ptr<const std::string> cached = lookupTextResponseCache(
                    cacheKey, serverData.isDevMode(), serverData.getTextResponseCacheMaxEntryBytes(),
                    serverData.getTextResponseCacheMaxTotalBytes())) {
                if (!co_await sendSocketAsync(cached->data(), cached->size())) {
                    sendToLoggerError("Failed to send cached file: " + contentPath);
                }
                co_return;
            }
        }

        std::unique_ptr<ContentBuilder> contentBuilder;

        if (contentType == "text/html") {
            if (httpRequest && serverData.getBasicAuth().requiresAuth(httpRequest->getPathString())) {
                std::string authHeader = httpRequest->getHeader("authorization");

                if (!serverData.getBasicAuth().authenticate(httpRequest->getPathString(), authHeader)) {
                    std::string header = buildAuthHeader();
                    if (!co_await sendSocketAsync(header.c_str(), header.size())) {
                        sendToLoggerError("Failed to send auth header");
                    }
                    co_return;
                }
            }

            if (httpRequest) {
                if (auto denial = co_await checkPageGateDenialAsync(*httpRequest)) {
                    record4xxMetric();
                    denial->serializeTo(responseScratch_);
                    if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                        sendToLoggerError("Failed to send page gate denial for: " + httpRequest->getPathString());
                    }
                    co_return;
                }
            }

            contentBuilder = std::make_unique<HtmlBuilder>(contentPath, serverData);

        } else if (contentType == "text/javascript") {
            contentBuilder = std::make_unique<JSBuilder>(contentPath, serverData);
        } else if (contentType == "text/css") {
            contentBuilder = std::make_unique<CSSBuilder>(contentPath, serverData);
        }

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
        if (!perRequestHtml) {
            storeTextResponseCache(cacheKey, contentPath, responseScratch_, serverData.isDevMode(),
                                   serverData.getTextResponseCacheMaxEntryBytes(),
                                   serverData.getTextResponseCacheMaxTotalBytes());
        }

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send file: " + contentPath);
        }

    } else {
        if (contentType == "image/webp" && serverData.isDevMode() && serverData.getWebPConversion()) {
            auto cachedWebP = HtmlBuilder::getWebPFromCache(contentPath);
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
                            auto webpData = WebPConverter::getFromCache(contentPath);
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

                            const std::string origType = getContentType(getExtension(sourcePath));
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

std::string Handler::getExtension(const std::string& path) const {
    // Check if path is for a file or a page
    if (path.find('.') == std::string::npos) {
        return "html";
    } else {
        // Get the extension of the file
        return path.substr(path.find('.') + 1);
    }
}

/**
 * Check if the path starts with a language prefix
 * @param str
 * @return true if it starts with a language prefix
 */
bool startsWithLangPrefix(const std::string& str) {
    return str.size() >= 4 && str[0] == '/' && std::isalpha(str[1]) && std::isalpha(str[2]) && str[3] == '/';
}

std::string Handler::buildPath(std::string& pathReceived, const std::string& Extension, HTTPRequest* httpRequest) const {
    // Block directory-traversal attempts before any path manipulation.
    if (!Security::isSafePath(serverData.getRoot(), pathReceived)) {
        sendToLoggerError("Path traversal attempt blocked: " + pathReceived);
        return "";
    }

    std::string htmlMount = "/html";

    // Check if specific language is requested

    const std::string_view language = httpRequest->getHeaderView("accept-language");

    // TODO : Find better way to check for language, can't always add an 'or' statement for each new language
    if (Extension == "jpg" || Extension == "jpeg" || Extension == "png" || Extension == "gif" || Extension == "svg" ||
        Extension == "ico" || Extension == "webp") {
        // For image files with /assets/ prefix, use path as-is (already normalized)
        if (pathReceived.find("/assets/") == 0) {
            // Assets are stored without language prefix, use direct path
            return serverData.getRoot() + pathReceived;
        }
        
        // For other image files, use the Referer header to determine the correct relative path

        // Try to get the context from the Referer header
        const std::string_view refererSv = httpRequest->getHeaderView("referer");
        if (!refererSv.empty()) {
            std::string referer(refererSv);

            // Remove everything after the last '/' from referer to get base path
            size_t lastSlashInReferer = referer.find_last_of('/');
            if (lastSlashInReferer != std::string::npos) {
                std::string refererBase = referer.substr(0, lastSlashInReferer + 1);  // Keep the trailing '/'

                // Build the full request URL for comparison using the Host header
                std::string host = "localhost";
                const std::string_view hostSv = httpRequest->getHeaderView("host");
                if (!hostSv.empty()) {
                    host.assign(hostSv);
                }
                std::string scheme = "http";
                const std::string_view forwardedProto = httpRequest->getHeaderView("x-forwarded-proto");
                if (!forwardedProto.empty()) {
                    scheme.assign(forwardedProto);
                    // Defensive: only allow "http" or "https"
                    for (char& ch : scheme) {
                        if (ch >= 'A' && ch <= 'Z') {
                            ch = static_cast<char>(ch + ('a' - 'A'));
                        }
                    }
                    if (scheme != "http" && scheme != "https") {
                        scheme = "http";
                    }
                }
                std::string fullRequestUrl = scheme + "://" + host + pathReceived;

                // Remove the referer base from the request URL to get relative path
                if (fullRequestUrl.find(refererBase) == 0) {
                    std::string relativePath = fullRequestUrl.substr(refererBase.length());
                    pathReceived = "/" + relativePath;
                } else {
                    // Fallback: if pattern doesn't match, return path as-is
                    // This preserves the original path structure including language prefixes
                    // pathReceived remains unchanged
                }
            } else {
                // Fallback: extract just filename
                // This preserves the original path structure including language prefixes
                // pathReceived remains unchanged
            }
        } else {
            // Remove the language prefix from the path

            size_t lastSlash = pathReceived.find_last_of('/');
            pathReceived = (lastSlash != std::string::npos) ? pathReceived.substr(lastSlash) : "/" + pathReceived;
        }
    } else if (!startsWithLangPrefix(pathReceived)) {
        if (pathReceived.size() == 1) {
            // if the size of the pathReceived is only 1 character long
            // then we can assume that the path is a language indicator for the index page

            // Only use language routing if languages are configured
            if (serverData.hasLanguages()) {
                // Extract the preferred language from Accept-Language header
                std::string preferredLang = serverData.getDefaultLanguage();

                // Try to find a matching language from the Accept-Language header
                for (const auto& lang : serverData.getAvailableLanguages()) {
                    if (language.find(lang) != std::string_view::npos) {
                        preferredLang = lang;
                        break;
                    }
                }

                // Always return language-specific path - HTMLBuilder will handle building if needed
                return serverData.getRoot() + "/html/" + preferredLang + "/index.html";
            }

            // Fallback to simple index.html if no languages configured
            return serverData.getRoot() + "/html/index.html";
        }

        // Try language-specific directories first, then fallback to /html
        if (serverData.hasLanguages()) {
            // Extract the preferred language from Accept-Language header
            std::string preferredLang = serverData.getDefaultLanguage();

            // Try to find a matching language from the Accept-Language header
            for (const auto& lang : serverData.getAvailableLanguages()) {
                if (language.find(lang) != std::string_view::npos) {
                    preferredLang = lang;
                    break;
                }
            }

            // Always use language-specific path when languages are configured
            // HTMLBuilder will handle loading from template if language file doesn't exist
            htmlMount = "/html/" + preferredLang;
        } else {
            // No languages configured, use simple /html
            htmlMount = "/html";
        }
    } else if (pathReceived.size() == 4) {
        // if the size of the pathReceived has a language indicator and is only 4 characters long
        // then we can assume that the path is a language indicator for the index page

        pathReceived += "/index";

    } else if (Extension != "html" && Extension != "htm") {
        // Remove the language prefix from the path
        pathReceived = pathReceived.substr(3);
    }

    if (pathReceived.find('.') == std::string::npos) {
        pathReceived += ".html";
    }

    std::string mount;
    if (Extension == "html" || Extension == "htm") {
        mount = htmlMount;
    } else {
        const auto& roots = handlerAssetRootByExtension();
        auto        it    = roots.find(Extension);
        if (it == roots.end()) {
            return "";
        }
        mount = it->second;
    }

    std::string finalPath = serverData.getRoot() + mount + pathReceived;
    if (!Security::isSafePath(serverData.getRoot(), mount + pathReceived)) {
        sendToLoggerError("Path traversal attempt blocked after assembly: " + finalPath);
        return "";
    }
    return finalPath;
}

/**
 * Get the content type for a file extension
 * @param extension
 * @return
 */
std::string Handler::getContentType(const std::string& extension) {
    const auto& types = handlerContentTypeByExtension();
    auto        it    = types.find(extension);
    return it != types.end() ? it->second : "application/octet-stream";
}

}  // namespace geruest
