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

#include "builders/ContentBuilder.hpp"
#include "builders/CSSBuilder.hpp"
#include "builders/HTMLBuilder.hpp"
#include "builders/JSBuilder.hpp"

namespace geruest {

#ifdef _WIN32
Handler::Handler(SOCKET socket, std::string IP, ServerData serverData)
#else
Handler::Handler(int socket, std::string IP, ServerData serverData)
#endif
    : clientSocket(socket), IP(std::move(IP)), serverData(serverData) {
    buffer = new char[BUFFER_SIZE];
}

Handler::~Handler() {
#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif
    delete[] buffer;

    //    logger.log("Client socket closed.");
}

bool Handler::readSocket(char *bufferToUse, size_t size) {
    // test if size is bigger than int
    if (size > INT_MAX) {
        sendToLoggerError("Size of buffer is too big.");
        return false;
    }

#ifdef _WIN32
    bufferLength = recv(clientSocket, bufferToUse, static_cast<int>(size), 0);
#else
    bufferLength = read(clientSocket, bufferToUse, static_cast<int>(size));
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
            sendToLogger("Timeout for receiving data.");
            idling++;
            if (idling > 2) {
                std::cout << "Client is idling too long. Closing connection." << std::endl;
            }
        }
        sendToLogger("Error reading from socket.");
    } else {
        return true;
    }

    return false;
}

bool Handler::sendSocket(const char *bufferToSend, size_t size) const {
    char bufferToSocket[BUFFER_SIZE];

    size_t startPos = 0;
    size_t dataSize = size;

    while (startPos < dataSize) {
        size_t chunkSize = std::min(static_cast<size_t>(BUFFER_SIZE), dataSize - startPos);
        std::memcpy(bufferToSocket, bufferToSend + startPos, chunkSize);
#ifdef _WIN32
        send(clientSocket, bufferToSocket, static_cast<int>(chunkSize), 0);
#else
        write(clientSocket, bufferToSocket, static_cast<int>(chunkSize));
#endif
        startPos += chunkSize;
    }

    return true;
}

void Handler::sendToLogger(const std::string &message) const {
    std::cout << "Log: " << message << " from " << IP << std::endl;
}
void Handler::sendToLoggerPages(const std::string &message) const {
    std::cout << "Page Log: " << message << " from " << IP << std::endl;
}
void Handler::sendToLoggerAPI(const std::string &message) const {
    std::cout << "API Log: " << message << " from " << IP << std::endl;
}
void Handler::sendToLoggerUser(const std::string &message) const {
    std::cout << "User Log: " << message << " from " << IP << std::endl;
}
void Handler::sendToLoggerError(const std::string &message) const {
    std::cerr << "Error Log: " << message << " from " << IP << std::endl;
}

void Handler::run() {
    // Read the socket
    // Message count is used to prevent infinite loops
    while (++messageCount < 100 && readSocket()) {
        // Check if the buffer exists
        if (buffer == nullptr) {
            buffer = new char[BUFFER_SIZE];
        }
        
        // Ensure bufferLength is valid before creating string
        if (bufferLength <= 0) {
            sendToLoggerError("Invalid buffer length in run loop.");
            break;
        }
        
        std::string rawRequest(buffer, static_cast<size_t>(bufferLength));
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
            std::string newData(buffer, static_cast<size_t>(bufferLength));
            requestStream.str(requestStream.str() + newData);

            hTTPRequest = HTTPRequest(requestStream.str(), IP, serverData.getRoot());
        }

        handleRequest(&hTTPRequest);

        // Clear the buffer, so we don't send the same data again
        memset(buffer, 0, BUFFER_SIZE);
    }
}

void Handler::handleRequest(HTTPRequest *request) {
    if (request == nullptr) {
        sendToLoggerError("HTTPRequest is null.");
        std::string header = buildInternalServerErrorHeader();
        sendSocket(header.c_str(), header.size());
        return;
    }

    auto it = serverData.getRoutes().find(request->getPathString());
    if (it != serverData.getRoutes().end()) {
        
        // Call the route handler
        HTTPResponse response = it->second(*request);

        // Send the response
        sendSocket(response.toString().c_str(), response.toString().size());

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

    sendSocket(header.c_str(), header.size());
}

// TODO : Redo with HTTPResponse
/**
 * Send a response
 * @param status
 * @param contentType
 * @param content
 */
void Handler::sendResponse(const std::string &status, const std::string &contentType,
                           const std::string &content) const {
    std::string response = buildHeader(status, contentType, std::to_string(content.size()));

    response += content;

    sendSocket((char *)response.c_str(), response.size());
}

/**
 * Send a file
 * @param contentType
 * @param contentPath
 */
void Handler::sendFile(const std::string &contentType, const std::string &contentPath, HTTPRequest *httpRequest) const {
    char bufferToSend[BUFFER_SIZE];

    // sendToLoggerPages("Sending file: " + contentPath);

    if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
        ContentBuilder *contentBuilder = nullptr;

        if (contentType == "text/html") {
            //			sendToLoggerPages("GET: " + path);

            int access = 1;  // hasAccess(requestStream.str(), httpRequest->getPathString(),
            // serverData->getRoot());

            if (access == 0) {
                std::string header = buildAuthHeader();
                sendSocket(header.c_str(), header.size());
                return;
            }

            if (access == -1) {
                std::string header = buildForbiddenHeader();
                sendSocket(header.c_str(), header.size());
                return;
            }

            contentBuilder = new HtmlBuilder(contentPath, serverData.getRoot());

        } else if (contentType == "text/javascript") {
            contentBuilder = new JSBuilder(contentPath, serverData.getRoot());
        } else if (contentType == "text/css") {
            contentBuilder = new CSSBuilder(contentPath, serverData.getRoot());
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

        std::string header = buildHeader("200 OK", contentType, contentBuilder->sizeString());

        std::string bufferToSocket = header + contentBuilder->file();

        sendSocket(bufferToSocket.c_str(), bufferToSocket.size());

        delete contentBuilder;

    } else {
        // Open file
        std::ifstream file(contentPath, std::ios::binary);

        if (!file.is_open()) {
            sendToLogger("File not found: " + contentPath);
            sendResponse("404 Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
            return;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        // sendToLogger("File size: " + std::to_string(fileSize));

        std::string response = buildHeader("200 OK", contentType, std::to_string(fileSize));

        // Send response
        sendSocket((char *)response.c_str(), response.size());

        while (!file.eof()) {
            file.read(bufferToSend, BUFFER_SIZE);
            sendSocket(bufferToSend, static_cast<size_t>(file.gcount()));
        }

        file.close();
    }
}

std::string Handler::getExtension(const std::string &path) {
    // Check if path is for a file or a page
    if (path.find('.') == std::string::npos) {
        return "html";
    } else {
        // Get the extension of the file
        return path.substr(path.find('.') + 1);
    }
}

// void Handler::removeSearchParameters() {
//
//	if (path.find('?') != std::string::npos) {
//		path = path.substr(0, path.find('?'));
//	}
// }

/**
 * Check if the path starts with a language prefix
 * @param str
 * @return true if it starts with a language prefix
 */
bool startsWithLangPrefix(const std::string &str) {
    return str.size() >= 4 && str[0] == '/' && std::isalpha(str[1]) && std::isalpha(str[2]) && str[3] == '/';
}

std::string Handler::buildPath(std::string &pathReceived, const std::string &Extension, HTTPRequest *httpRequest) {
    std::map<std::string, std::string> contentRoot = {
        {"html", "/html"},         {"htm", "/html"},           {"css", "/assets/css"},    {"js", "/assets/js"},
        {"jpg", "/assets/images"}, {"jpeg", "/assets/images"}, {"png", "/assets/images"}, {"gif", "/assets/images"},
        {"svg", "/assets/images"}, {"ico", "/assets/images"},  {"JSON", "/assets/JSONs"}, {"pdf", "/assets/docs"},
        {"zip", "/assets/docs"},   {"mp3", "/assets/audio"},   {"mp4", "/assets/video"},  {"xml", "/assets/docs"},
        {"csv", "/assets/docs"},   {"txt", "/assets/docs"}};

    // Check if specific language is requested

    std::string language = httpRequest->hasHeader("Accept-Language") ? httpRequest->getHeader("Accept-Language") : "";

    // TODO : Find better way to check for language, can't always add an 'or' statement for each new language
    if (!startsWithLangPrefix(pathReceived)) {
        if (pathReceived.size() == 1) {
            // if the size of the pathReceived is only 1 character long
            // then we can assume that the path is a language indicator for the index page

            if (language.find("de") != std::string::npos) {
                return serverData.getRoot() + "/html/de/index.html";
            } else if (language.find("fr") != std::string::npos) {
                return serverData.getRoot() + "/html/fr/index.html";
            }

            return serverData.getRoot() + "/html/en/index.html";
        }

        if (language.find("de") != std::string::npos) {
            contentRoot["html"] = "/html/de";
        } else if (language.find("fr") != std::string::npos) {
            contentRoot["html"] = "/html/fr";
        } else {
            contentRoot["html"] = "/html/en";
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
std::string Handler::getContentType(const std::string &extension) {
    std::map<std::string, std::string> contentTypes = {
        {"html", "text/html"},      {"htm", "text/html"},    {"css", "text/css"},          {"js", "text/javascript"},
        {"jpg", "image/jpeg"},      {"jpeg", "image/jpeg"},  {"png", "image/png"},         {"gif", "image/gif"},
        {"svg", "image/svg+xml"},   {"ico", "image/x-icon"}, {"JSON", "application/JSON"}, {"pdf", "application/pdf"},
        {"zip", "application/zip"}, {"mp3", "audio/mpeg"},   {"mp4", "video/mp4"},         {"xml", "application/xml"},
        {"csv", "text/csv"},        {"txt", "text/plain"}};

    return contentTypes.count(extension) ? contentTypes[extension] : "application/octet-stream";
}

}  // namespace geruest
