/**
 * @file HTTPResponse.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This function is used to build HTTP headers.
 */

#include "HTTPResponse.hpp"

namespace geruest {

HTTPResponse::HTTPResponse(const std::string& statusCode) : status(statusCode) {
    // Default headers
    headers["Content-Type"] = "text/html";
    headers["Connection"] = "Keep-Alive";
    headers["Keep-Alive"] = "timeout=5, max=100";
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void HTTPResponse::setBody(const std::string& responseBody) {
    body = responseBody;
    headers["Content-Length"] = std::to_string(body.size());
}

std::string HTTPResponse::toString() const {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n";

    for (const auto& header : headers) {
        response << header.first << ": " << header.second << "\r\n";
    }

    response << "\r\n" << body;
    return response.str();
}

std::string buildHeader(const std::string &status,
                       const std::string &contentType,
                       const std::string &size) {
    HTTPResponse response(status);
    response.setHeader("Content-Type", contentType);
    response.setHeader("Content-Length", size);
    return response.toString();
}

std::string buildBadRequestHeader() {
    HTTPResponse response("400 Bad Request");
    response.setBody("400 Bad Request");
    return response.toString();
}

std::string buildAuthHeader() {
    HTTPResponse response("401 Unauthorized");
    response.setHeader("WWW-Authenticate", "Basic realm=\"Restricted Area\"");
    response.setBody("401 Unauthorized");
    return response.toString();
}

std::string buildForbiddenHeader() {
    HTTPResponse response("403 Forbidden");
    response.setBody("403 Forbidden");
    return response.toString();
}

std::string buildNotFoundHeader() {
    HTTPResponse response("404 Not Found");
    response.setBody("404 Not Found");
    return response.toString();
}

std::string buildMethodNotAllowedHeader() {
    HTTPResponse response("405 Method Not Allowed");
    response.setBody("405 Method Not Allowed");
    return response.toString();
}

std::string buildOKHeader() {
    HTTPResponse response("200 OK");
    response.setBody("200 OK");
    return response.toString();
}

std::string buildFailHeader() {
    HTTPResponse response("500 Internal Server Error");
    response.setBody("500 Internal Server Error");
    return response.toString();
}

std::string buildInternalServerErrorHeader() {
    HTTPResponse response("500 Internal Server Error");
    response.setBody("500 Internal Server Error");
    return response.toString();
}

}  // namespace geruest
