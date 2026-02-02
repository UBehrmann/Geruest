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
#include <climits>
#include <filesystem>

#include "builders/AssetMerger.hpp"
#include "builders/CSSBuilder.hpp"
#include "builders/ContentBuilder.hpp"
#include "builders/HTMLBuilder.hpp"
#include "builders/JSBuilder.hpp"
#include "builders/WebPConverter.hpp"
#include "data/HTTPResponse.hpp"

namespace geruest {

#ifdef _WIN32
Handler::Handler(SOCKET socket, std::string clientIP, const ServerData& serverDataRef)
#else
Handler::Handler(int socket, std::string clientIP, const ServerData& serverDataRef)
#endif
    : clientSocket(socket), serverData(serverDataRef), IP(std::move(clientIP)), buffer(std::make_unique<char[]>(BUFFER_SIZE)) {
}

Handler::~Handler() {
#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif

    //    logger.log("Client socket closed.");
}

bool Handler::readSocket(char* bufferToUse, size_t size) {
    // test if size is bigger than int
    if (size > INT_MAX) {
        sendToLoggerError("Size of buffer is too big.");
        return false;
    }

#ifdef _WIN32
    bufferLength = recv(clientSocket, bufferToUse, static_cast<int>(size), 0);
#else
    bufferLength = read(clientSocket, bufferToUse, size);
#endif

    if (bufferLength == 0) {
        // sendToLogger("Client disconnected.");
    } else if (bufferLength < 0) {
        // Check if the error is due to timeout
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
            sendToLogger("Timeout for receiving data.", LogLevel::Warning);
            idling++;
            if (idling > 2) {
                std::cout << "Client is idling too long. Closing connection." << std::endl;
            }
        }
        sendToLogger("Error reading from socket.", LogLevel::Warning);
    } else {
        return true;
    }

    return false;
}

bool Handler::sendSocket(const char* bufferToSend, size_t size) const {
    char bufferToSocket[BUFFER_SIZE];

    size_t startPos = 0;
    size_t dataSize = size;

    while (startPos < dataSize) {
        size_t chunkSize = std::min(static_cast<size_t>(BUFFER_SIZE), dataSize - startPos);
        std::memcpy(bufferToSocket, bufferToSend + startPos, chunkSize);
        
        ssize_t bytesSent;
#ifdef _WIN32
        bytesSent = send(clientSocket, bufferToSocket, static_cast<int>(chunkSize), 0);
#else
        bytesSent = write(clientSocket, bufferToSocket, chunkSize);
#endif
        
        if (bytesSent < 0) {
            // Error sending data
            return false;
        }
        
        startPos += chunkSize;
    }

    return true;
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

void Handler::run() {
    // Read the socket
    // Message count is used to prevent infinite loops
    while (++messageCount < 100 && readSocket()) {
        // Check if the buffer exists
        if (!buffer) {
            buffer = std::make_unique<char[]>(BUFFER_SIZE);
        }

        // Ensure bufferLength is valid before creating string
        if (bufferLength <= 0) {
            sendToLoggerError("Invalid buffer length in run loop.");
            break;
        }

        std::string rawRequest(buffer.get(), static_cast<size_t>(bufferLength));
        requestStream = std::istringstream(rawRequest);

        HTTPRequest hTTPRequest(rawRequest, IP, serverData.getRoot());

        // Check if body was read with the request, otherwise it was sent in the next read
        if (hTTPRequest.hasHeader("content-length") && hTTPRequest.getBody().empty()) {
            readSocket();

            // Ensure bufferLength is valid before creating string
            if (bufferLength <= 0) {
                sendToLoggerError("Invalid buffer length when reading body.");
                break;
            }

            // Append the new data to the existing buffer
            std::string newData(buffer.get(), static_cast<size_t>(bufferLength));
            requestStream.str(requestStream.str() + newData);

            hTTPRequest = HTTPRequest(requestStream.str(), IP, serverData.getRoot());
        }

        handleRequest(&hTTPRequest);

        // Clear the buffer, so we don't send the same data again
        memset(buffer.get(), 0, BUFFER_SIZE);
    }
}

void Handler::handleRequest(HTTPRequest* request) {
    if (request == nullptr) {
        sendToLoggerError("HTTPRequest is null.");
        std::string header = buildInternalServerErrorHeader();
        if (!sendSocket(header.c_str(), header.size())) {
            sendToLoggerError("Failed to send internal server error response");
        }
        return;
    }

    // Try to find a matching route (exact or wildcard)
    auto routeHandler = serverData.findMatchingRoute(request->getPathString());
    if (routeHandler) {
        // Call the route handler  
        HTTPResponse response = (*routeHandler)(*request);

        // Send the response
        std::string responseStr = response.toString();
        if (!sendSocket(responseStr.c_str(), responseStr.size())) {
            sendToLoggerError("Failed to send route response for: " + request->getPathString());
        }

        return;

    } else {
        std::string path = request->getPathString();

        // Here html logic should be added
        std::string extension = getExtension(path);
        std::string content_type = getContentType(extension);
        std::string contentPath = buildPath(path, extension, request);

        sendFile(content_type, contentPath, request);

        return;
    }

    sendToLoggerPages("Not found: " + request->getPathString());

    std::string header = buildNotFoundHeader();

    if (!sendSocket(header.c_str(), header.size())) {
        sendToLoggerError("Failed to send 404 response");
    }
}

// TODO : Redo with HTTPResponse
/**
 * Send a response
 * @param status
 * @param contentType
 * @param content
 */
void Handler::sendResponse(const std::string& status, const std::string& contentType,
                           const std::string& content) const {
    std::string response = buildHeader(status, contentType, std::to_string(content.size()));

    response += content;

    if (!sendSocket((char*)response.c_str(), response.size())) {
        sendToLoggerError("Failed to send response: " + status);
    }
}

/**
 * Send a file
 * @param contentType
 * @param contentPath
 */
void Handler::sendFile(const std::string& contentType, const std::string& contentPath, HTTPRequest* httpRequest) const {
    char bufferToSend[BUFFER_SIZE];

    // sendToLoggerPages("Sending file: " + contentPath);

    if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
        std::unique_ptr<ContentBuilder> contentBuilder;

        if (contentType == "text/html") {
            //			sendToLoggerPages("GET: " + path);

            // Check Basic Authentication if required for this page
            if (httpRequest && serverData.getBasicAuth().requiresAuth(httpRequest->getPathString())) {
                std::string authHeader = httpRequest->getHeader("authorization");
                
                if (!serverData.getBasicAuth().authenticate(httpRequest->getPathString(), authHeader)) {
                    // Authentication required but failed
                    std::string header = buildAuthHeader();
                    if (!sendSocket(header.c_str(), header.size())) {
                        sendToLoggerError("Failed to send auth header");
                    }
                    return;
                }
            }

            contentBuilder = std::make_unique<HtmlBuilder>(contentPath, serverData);

        } else if (contentType == "text/javascript") {
            contentBuilder = std::make_unique<JSBuilder>(contentPath, serverData);
        } else if (contentType == "text/css") {
            contentBuilder = std::make_unique<CSSBuilder>(contentPath, serverData);
        }

        // Check if builder was created
        if (!contentBuilder) {
            // sendToLogger("File not found: " + contentPath);
            sendResponse("404 Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
            return;
        }

        // Check if content is empty
        if (contentBuilder->size() == 0) {
            sendResponse("404 Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
            return;
        }

        HTTPResponse htmlResponse("200 OK");

        htmlResponse.setHeader("Content-Type", contentType);

        htmlResponse.setBody(contentBuilder->file());

        std::string bufferToSocket = htmlResponse.toString();

        if (!sendSocket(bufferToSocket.c_str(), bufferToSocket.size())) {
            sendToLoggerError("Failed to send file: " + contentPath);
        }


    } else {
        // Check if this is a WebP request and we have it cached (devMode)
        if (contentType == "image/webp" && serverData.isDevMode() && serverData.getWebPConversion()) {
            // Try to get from WebP cache
            std::vector<uint8_t> cachedWebP = HtmlBuilder::getWebPFromCache(contentPath);
            if (!cachedWebP.empty()) {
                // Serve from cache
                HTTPResponse htmlResponse("200 OK");
                htmlResponse.setHeader("Content-Type", contentType);
                htmlResponse.setHeader("Content-Length", std::to_string(cachedWebP.size()));
                
                std::string response = htmlResponse.toString();
                
                // Send headers
                if (!sendSocket(response.c_str(), response.size())) {
                    sendToLoggerError("Failed to send cached WebP header: " + contentPath);
                    return;
                }
                
                // Send cached WebP data
                if (!sendSocket(reinterpret_cast<const char*>(cachedWebP.data()), cachedWebP.size())) {
                    sendToLoggerError("Failed to send cached WebP data: " + contentPath);
                }
                return;
            }
        }
        
        // Open file
        std::ifstream file(contentPath, std::ios::binary);

        if (!file.is_open()) {
            // If this is a WebP request and the file doesn't exist, try on-demand conversion
            if (contentType == "image/webp" && serverData.getWebPConversion()) {
                // Look for original PNG/JPG/JPEG file
                std::string basePath = contentPath;
                size_t dotPos = basePath.find_last_of('.');
                if (dotPos != std::string::npos) {
                    basePath = basePath.substr(0, dotPos);
                }
                
                std::string sourcePath;
                std::vector<std::string> extensions = {".jpg", ".jpeg", ".png"};
                
                for (const auto& ext : extensions) {
                    if (std::filesystem::exists(basePath + ext)) {
                        sourcePath = basePath + ext;
                        break;
                    }
                }
                
                if (!sourcePath.empty()) {
                    // Convert on-demand
                    bool cacheOnly = serverData.isDevMode();
                    if (WebPConverter::convertImage(sourcePath, contentPath, cacheOnly, serverData.getWebPQuality())) {
                        if (cacheOnly) {
                            // Serve from cache
                            std::vector<uint8_t> webpData = WebPConverter::getFromCache(contentPath);
                            if (!webpData.empty()) {
                                HTTPResponse webpResponse("200 OK");
                                webpResponse.setHeader("Content-Type", contentType);
                                webpResponse.setHeader("Content-Length", std::to_string(webpData.size()));
                                
                                std::string response = webpResponse.toString();
                                
                                if (!sendSocket(response.c_str(), response.size())) {
                                    sendToLoggerError("Failed to send on-demand WebP header: " + contentPath);
                                    return;
                                }
                                
                                if (!sendSocket(reinterpret_cast<const char*>(webpData.data()), webpData.size())) {
                                    sendToLoggerError("Failed to send on-demand WebP data: " + contentPath);
                                }
                                return;
                            }
                        } else {
                            // File was saved to disk, try opening again
                            file.open(contentPath, std::ios::binary);
                            if (file.is_open()) {
                                // Continue with normal file serving below
                            }
                        }
                    }
                }
            }
            
            // If still not open, send 404
            if (!file.is_open()) {
                sendToLogger("File not found: " + contentPath);
                sendResponse("404 Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
                return;
            }
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        // sendToLogger("File size: " + std::to_string(fileSize));

        HTTPResponse htmlResponse("200 OK");

        htmlResponse.setHeader("Content-Type", contentType);
        htmlResponse.setHeader("Content-Length", std::to_string(fileSize));

        std::string response = htmlResponse.toString();

        // Send response
        if (!sendSocket((char*)response.c_str(), response.size())) {
            sendToLoggerError("Failed to send binary file header: " + contentPath);
            file.close();
            return;
        }

        while (!file.eof()) {
            file.read(bufferToSend, BUFFER_SIZE);
            if (!sendSocket(bufferToSend, static_cast<size_t>(file.gcount()))) {
                sendToLoggerError("Failed to send binary file chunk: " + contentPath);
                break;
            }
        }

        file.close();
    }
}

std::string Handler::getExtension(const std::string& path) {
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

std::string Handler::buildPath(std::string& pathReceived, const std::string& Extension, HTTPRequest* httpRequest) {
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

    return contentRoot.count(Extension) ? serverData.getRoot() + contentRoot[Extension] + pathReceived : "";
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
