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
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "builders/AssetMerger.hpp"
#include "builders/CSSBuilder.hpp"
#include "builders/ContentBuilder.hpp"
#include "builders/HTMLBuilder.hpp"
#include "builders/JSBuilder.hpp"
#include "builders/WebPConverter.hpp"
#include "data/HTTPResponse.hpp"
#include "data/MethodNotAllowed.hpp"
#include "security/Security.hpp"

namespace {

constexpr size_t kMaxHttpHeaderBytes = 65536;
constexpr size_t kMaxHttpBodyBytes = 16 * 1024 * 1024;

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

bool parseContentLengthBytes(const geruest::HTTPRequest& req, size_t* out) {
    if (!req.hasHeader("content-length")) {
        return false;
    }
    const std::string cl = req.getHeader("content-length");
    try {
        unsigned long long v = std::stoull(cl);
        *out = static_cast<size_t>(v);
        return true;
    } catch (...) {
        return false;
    }
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

boost::asio::awaitable<bool> Handler::readSocketAsync(char* bufferToUse, size_t size) {
    if (size > INT_MAX) {
        sendToLoggerError("Size of buffer is too big.");
        co_return false;
    }

    try {
        const std::size_t n = co_await clientSocket.async_read_some(
            boost::asio::buffer(bufferToUse, size), boost::asio::use_awaitable);
        bufferLength = static_cast<std::int64_t>(n);
    } catch (const boost::system::system_error&) {
        sendToLogger("Error reading from socket.", LogLevel::Warning);
        co_return false;
    }

    if (bufferLength == 0) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> Handler::readSocketAsync() { co_return co_await readSocketAsync(buffer.get(), BUFFER_SIZE); }

boost::asio::awaitable<bool> Handler::discardFromSocketAsync(size_t byteCount) {
    while (byteCount > 0) {
        const size_t chunk = std::min(byteCount, static_cast<size_t>(BUFFER_SIZE));
        if (!co_await readSocketAsync(buffer.get(), chunk)) {
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

    while (++messageCount < 100) {
        std::string raw = std::move(pendingRequestData);
        pendingRequestData.clear();

        if (raw.empty()) {
            if (!co_await readSocketAsync()) {
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
            if (!co_await readSocketAsync()) {
                co_return;
            }
            if (bufferLength <= 0) {
                sendToLoggerError("Invalid buffer length while reading headers.");
                co_return;
            }
            raw.append(buffer.get(), static_cast<size_t>(bufferLength));
        }

        const size_t headerEnd = findHeaderEndPos(raw);

        // RFC 7231: clients may send Expect: 100-continue and wait (e.g. httpx POST with Content-Length: 0).
        // Respond before reading the body or finalizing the message so the client does not stall.
        {
            HTTPRequest headOnly(raw.substr(0, headerEnd), IP, serverData.getRoot());
            if (headOnly.hasHeader("expect") && httpExpectIs100Continue(headOnly.getHeader("expect"))) {
                static const char k100[] = "HTTP/1.1 100 Continue\r\n\r\n";
                if (!co_await sendSocketAsync(k100, sizeof(k100) - 1)) {
                    co_return;
                }
            }
        }

        bool hasCL = false;
        size_t bodyExpected = 0;
        {
            HTTPRequest probe(raw, IP, serverData.getRoot());
            if (probe.hasHeader("content-length")) {
                if (!parseContentLengthBytes(probe, &bodyExpected)) {
                    HTTPResponse br = responseBadRequest(&probe);
                    const std::string s = br.toString();
                    co_await sendSocketAsync(s.c_str(), s.size());
                    co_return;
                }
                hasCL = true;
            }
        }

        if (hasCL && bodyExpected > kMaxHttpBodyBytes) {
            HTTPRequest probe(raw, IP, serverData.getRoot());
            HTTPResponse br = responseBadRequest(&probe);
            const std::string s = br.toString();
            co_await sendSocketAsync(s.c_str(), s.size());
            const size_t already = raw.size() > headerEnd ? raw.size() - headerEnd : 0;
            const size_t remain = bodyExpected > already ? bodyExpected - already : 0;
            static_cast<void>(co_await discardFromSocketAsync(remain));
            co_return;
        }

        const size_t needTotal = headerEnd + (hasCL ? bodyExpected : 0);
        while (raw.size() < needTotal) {
            if (!co_await readSocketAsync()) {
                co_return;
            }
            if (bufferLength <= 0) {
                sendToLoggerError("Invalid buffer length when reading body.");
                co_return;
            }
            raw.append(buffer.get(), static_cast<size_t>(bufferLength));
        }

        std::string message = raw.substr(0, needTotal);
        if (raw.size() > needTotal) {
            pendingRequestData = raw.substr(needTotal);
        }

        HTTPRequest hTTPRequest(message, IP, serverData.getRoot());
        requestStream = std::istringstream(message);

        serverData.recordRequest();
        {
            const auto _reqStart = std::chrono::steady_clock::now();
            co_await handleRequestAsync(&hTTPRequest);
            const auto _elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::steady_clock::now() - _reqStart)
                                        .count();
            serverData.recordLatency(_elapsedUs <= 0 ? 0u
                                     : _elapsedUs > 0xFFFFFFFFLL ? 0xFFFFFFFFu
                                                                 : static_cast<uint32_t>(_elapsedUs));
        }

        memset(buffer.get(), 0, BUFFER_SIZE);
    }
    co_return;
}

boost::asio::awaitable<void> Handler::handleRequestAsync(HTTPRequest* request) {
    if (request == nullptr) {
        serverData.recordError();
        sendToLoggerError("HTTPRequest is null.");
        std::string header = buildInternalServerErrorHeader();
        if (!co_await sendSocketAsync(header.c_str(), header.size())) {
            sendToLoggerError("Failed to send internal server error response");
        }
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

        std::string responseStr = redirectResponse.toString();
        if (!co_await sendSocketAsync(responseStr.c_str(), responseStr.size())) {
            sendToLoggerError("Failed to send redirect response for: " + request->getPathString());
        }
        co_return;
    }

    // Priority rule 3+4: normal routes (exact route, then wildcard route)
    auto routeHandler = serverData.findMatchingRoute(request->getPathString());
    if (routeHandler) {
        // co_await is not allowed inside catch clauses; build the body first, send once.
        std::string responseStr;
        const char* failLog = nullptr;
        try {
            HTTPResponse response = (*routeHandler)(*request);

            const std::string& _st = response.getStatus();
            if (!_st.empty()) {
                if (_st[0] == '4') {
                    serverData.record4xx();
                } else if (_st[0] == '5') {
                    serverData.record5xx();
                }
            }

            responseStr = response.toString();
            failLog     = "Failed to send route response for: ";
        } catch (const method_not_allowed& e) {
            HTTPResponse response = responseMethodNotAllowed(request, e.allowMethods());
            serverData.record4xx();
            responseStr = response.toString();
            failLog     = "Failed to send 405 for: ";
        } catch (const std::exception& e) {
            sendToLoggerError(std::string("Exception in route handler: ") + e.what());
            HTTPResponse response = responseInternalServerError(request);
            serverData.record5xx();
            responseStr = response.toString();
            failLog     = "Failed to send 500 for: ";
        } catch (...) {
            sendToLoggerError("Unknown exception in route handler");
            HTTPResponse response = responseInternalServerError(request);
            serverData.record5xx();
            responseStr = response.toString();
            failLog     = "Failed to send 500 for: ";
        }

        if (!co_await sendSocketAsync(responseStr.c_str(), responseStr.size())) {
            sendToLoggerError(std::string(failLog) + request->getPathString());
        }

        co_return;

    } else {
        std::string path = request->getPathString();

        // Here html logic should be added
        std::string extension = getExtension(path);
        std::string content_type = getContentType(extension);
        std::string contentPath = buildPath(path, extension, request);

        co_await sendFileAsync(content_type, contentPath, request);
    }
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
    std::string response = buildHeader(status, contentType, std::to_string(content.size()));

    response += content;

    if (!co_await sendSocketAsync(response.c_str(), response.size())) {
        sendToLoggerError("Failed to send response: " + status);
    }
    co_return;
}

boost::asio::awaitable<void> Handler::sendNotFoundResponseAsync(HTTPRequest* httpRequest) {
    serverData.record4xx();
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
    serverData.record5xx();
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

        std::string bufferToSocket = htmlResponse.toString();

        if (!co_await sendSocketAsync(bufferToSocket.c_str(), bufferToSocket.size())) {
            sendToLoggerError("Failed to send file: " + contentPath);
        }

    } else {
        if (contentType == "image/webp" && serverData.isDevMode() && serverData.getWebPConversion()) {
            auto cachedWebP = HtmlBuilder::getWebPFromCache(contentPath);
            if (cachedWebP && !cachedWebP->empty()) {
                HTTPResponse htmlResponse("200 OK");
                htmlResponse.setHeader("Content-Type", contentType);
                htmlResponse.setHeader("Content-Length", std::to_string(cachedWebP->size()));

                std::string response = htmlResponse.toString();

                if (!co_await sendSocketAsync(response.c_str(), response.size())) {
                    sendToLoggerError("Failed to send cached WebP header: " + contentPath);
                    co_return;
                }

                if (!co_await sendSocketAsync(reinterpret_cast<const char*>(cachedWebP->data()), cachedWebP->size())) {
                    sendToLoggerError("Failed to send cached WebP data: " + contentPath);
                }
                co_return;
            }
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

                                std::string response = webpResponse.toString();

                                if (!co_await sendSocketAsync(response.c_str(), response.size())) {
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
                            std::string origHeader = origResp.toString();

                            if (!co_await sendSocketAsync(origHeader.c_str(), origHeader.size())) {
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

        std::string response = htmlResponse.toString();

        if (!co_await sendSocketAsync(response.c_str(), response.size())) {
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

    std::map<std::string, std::string> contentRoot = {
        {"html", "/html"},         {"htm", "/html"},           {"css", "/assets/css"},    {"js", "/assets/js"},
        {"jpg", "/assets/images"}, {"jpeg", "/assets/images"}, {"png", "/assets/images"}, {"gif", "/assets/images"},
        {"svg", "/assets/images"}, {"ico", "/assets/images"},  {"webp", "/assets/images"}, {"JSON", "/assets/JSONs"}, {"pdf", "/assets/docs"},
        {"zip", "/assets/docs"},   {"mp3", "/assets/audio"},   {"mp4", "/assets/video"},  {"xml", "/assets/docs"},
        {"csv", "/assets/docs"},   {"txt", "/assets/docs"}};

    // Check if specific language is requested

    std::string language = httpRequest->hasHeader("Accept-Language") ? httpRequest->getHeader("Accept-Language") : "";

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
        if (httpRequest->hasHeader("Referer")) {
            std::string referer = httpRequest->getHeader("Referer");

            // Remove everything after the last '/' from referer to get base path
            size_t lastSlashInReferer = referer.find_last_of('/');
            if (lastSlashInReferer != std::string::npos) {
                std::string refererBase = referer.substr(0, lastSlashInReferer + 1);  // Keep the trailing '/'

                // Build the full request URL for comparison using the Host header
                std::string host = httpRequest->hasHeader("Host") ? httpRequest->getHeader("Host") : "localhost";
                std::string scheme = "http";
                if (httpRequest->hasHeader("X-Forwarded-Proto")) {
                    scheme = httpRequest->getHeader("X-Forwarded-Proto");
                    // Defensive: only allow "http" or "https"
                    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
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
                    if (language.find(lang) != std::string::npos) {
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
                if (language.find(lang) != std::string::npos) {
                    preferredLang = lang;
                    break;
                }
            }

            // Always use language-specific path when languages are configured
            // HTMLBuilder will handle loading from template if language file doesn't exist
            contentRoot["html"] = "/html/" + preferredLang;
        } else {
            // No languages configured, use simple /html
            contentRoot["html"] = "/html";
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

    if (!contentRoot.count(Extension)) return "";

    std::string finalPath = serverData.getRoot() + contentRoot[Extension] + pathReceived;
    if (!Security::isSafePath(serverData.getRoot(), contentRoot[Extension] + pathReceived)) {
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
    std::map<std::string, std::string> contentTypes = {
        {"html", "text/html"},      {"htm", "text/html"},    {"css", "text/css"},          {"js", "text/javascript"},
        {"jpg", "image/jpeg"},      {"jpeg", "image/jpeg"},  {"png", "image/png"},         {"gif", "image/gif"},
        {"webp", "image/webp"},     {"svg", "image/svg+xml"},{"ico", "image/x-icon"},      {"JSON", "application/JSON"},
        {"pdf", "application/pdf"}, {"zip", "application/zip"}, {"mp3", "audio/mpeg"},     {"mp4", "video/mp4"},
        {"xml", "application/xml"}, {"csv", "text/csv"},        {"txt", "text/plain"}};

    return contentTypes.count(extension) ? contentTypes[extension] : "application/octet-stream";
}

}  // namespace geruest
