/**
 * @file HTTPResponse.hpp
 * @date 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This function is used to build HTTP headers.
 */

#ifndef GERUEST_HTTPRESPONSE_HPP
#define GERUEST_HTTPRESPONSE_HPP

#include <string>
#include <unordered_map>
#include <sstream>

/**
 * @class HTTPResponse
 * @brief A class for building dynamic HTTP responses with customizable headers.
 */
class HTTPResponse {
private:
    std::string status;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

public:
    /**
     * Constructor to initialize the response with a status code.
     * @param statusCode The HTTP status code (e.g., "200 OK").
     */
    explicit HTTPResponse(const std::string& statusCode)
            : status(statusCode) {
        // Default headers
        headers["Content-Type"] = "text/html";
        headers["Connection"] = "Keep-Alive";
        headers["Keep-Alive"] = "timeout=5, max=100";
    }

    /**
     * Sets a custom header field.
     * @param key The header name.
     * @param value The header value.
     */
    void setHeader(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    /**
     * Sets the response body and updates the Content-Length header.
     * @param responseBody The response body.
     */
    void setBody(const std::string& responseBody) {
        body = responseBody;
        headers["Content-Length"] = std::to_string(body.size());
    }

    /**
     * Builds and returns the full HTTP response as a string.
     * @return The complete HTTP response.
     */
    std::string toString() const {
        std::ostringstream response;
        response << "HTTP/1.1 " << status << "\n";

        for (const auto& header : headers) {
            response << header.first << ": " << header.second << "\n";
        }

        response << "\n" << body;
        return response.str();
    }
};

/**
 * Static functions that mirror existing inline functions
 */

[[maybe_unused]] static std::string buildHeader(const std::string &status,
                               const std::string &contentType,
                               const std::string &size) {
    HTTPResponse response(status);
    response.setHeader("Content-Type", contentType);
    response.setHeader("Content-Length", size);
    return response.toString();
}

[[maybe_unused]] static std::string buildBadRequestHeader() {
    HTTPResponse response("400 Bad Request");
    response.setBody("400 Bad Request");
    return response.toString();
}

[[maybe_unused]] static std::string buildAuthHeader() {
    HTTPResponse response("401 Unauthorized");
    response.setHeader("WWW-Authenticate", "Basic realm=\"Restricted Area\"");
    response.setBody("401 Unauthorized");
    return response.toString();
}

[[maybe_unused]] static std::string buildForbiddenHeader() {
    HTTPResponse response("403 Forbidden");
    response.setBody("403 Forbidden");
    return response.toString();
}

[[maybe_unused]] static std::string buildNotFoundHeader() {
    HTTPResponse response("404 Not Found");
    response.setBody("404 Not Found");
    return response.toString();
}

[[maybe_unused]] static std::string buildMethodNotAllowedHeader() {
    HTTPResponse response("405 Method Not Allowed");
    response.setBody("405 Method Not Allowed");
    return response.toString();
}

[[maybe_unused]] static std::string buildOKHeader() {
    HTTPResponse response("200 OK");
    response.setBody("200 OK");
    return response.toString();
}

[[maybe_unused]] static std::string buildFailHeader() {
    HTTPResponse response("500 Internal Server Error");
    response.setBody("500 Internal Server Error");
    return response.toString();
}

[[maybe_unused]] static std::string buildInternalServerErrorHeader(){
    HTTPResponse response("500 Internal Server Error");
    response.setBody("500 Internal Server Error");
    return response.toString();
}

#endif // GERUEST_HTTPRESPONSE_HPP