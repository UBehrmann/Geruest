/**
 * @file HTTPResponse.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This function is used to build HTTP headers.
 */

#include "HTTPResponse.hpp"

#include <algorithm>
#include <string_view>

#include "HTTPRequest.hpp"

namespace geruest {

namespace {

void eraseHeaderKey(std::vector<std::pair<std::string, std::string>>& hdrs, const std::string& key) {
    std::erase_if(hdrs, [&key](const std::pair<std::string, std::string>& p) { return p.first == key; });
}

bool hasHeaderKey(const std::vector<std::pair<std::string, std::string>>& hdrs, const std::string& key) {
    return std::any_of(hdrs.begin(), hdrs.end(),
                       [&key](const std::pair<std::string, std::string>& p) { return p.first == key; });
}

}  // namespace

HTTPResponse::HTTPResponse(const std::string& statusCode) : status(statusCode) {
    headers.reserve(8);
    headers.emplace_back("Content-Type", "text/html");
    headers.emplace_back("Connection", "Keep-Alive");
    headers.emplace_back("Keep-Alive", "timeout=5, max=100");
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value) {
    eraseHeaderKey(headers, key);
    headers.emplace_back(key, value);
}

void HTTPResponse::addHeader(const std::string& key, const std::string& value) {
    headers.emplace_back(key, value);
}

void HTTPResponse::setBody(const std::string& responseBody) {
    body = responseBody;
    eraseHeaderKey(headers, "Content-Length");
    headers.emplace_back("Content-Length", std::to_string(body.size()));
}

void HTTPResponse::serializeTo(std::string& out) const {
    size_t estimatedSize = 32 + status.size() + body.size();
    for (const auto& header : headers) {
        estimatedSize += header.first.size() + header.second.size() + 4;
    }

    out.clear();
    out.reserve(estimatedSize);

    out += "HTTP/1.1 ";
    out += status;
    out += "\r\n";

    for (const auto& header : headers) {
        out += header.first;
        out += ": ";
        out += header.second;
        out += "\r\n";
    }

    if (!hasHeaderKey(headers, "Content-Length")) {
        out += "Content-Length: ";
        if (body.empty()) {
            out += "0";
        } else {
            out += std::to_string(body.size());
        }
        out += "\r\n";
    }

    out += "\r\n";
    out += body;
}

std::string HTTPResponse::toString() const {
    std::string response;
    serializeTo(response);
    return response;
}

std::string buildHeader(const std::string& status, const std::string& contentType, const std::string& size) {
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

// Inline functions that return HTTPResponse objects

void addDefaultHeaders(HTTPResponse* response, const HTTPRequest* request) {
    if (!response) return;
    response->setHeader("Connection", "Keep-Alive");
    response->setHeader("Keep-Alive", "timeout=5, max=100");
    response->setHeader("Cache-Control", "no-cache");
    response->setHeader("Content-Type", "text/plain");

    if (request && request->hasHeader("origin")) {
        response->setHeader("Access-Control-Allow-Origin", request->getHeader("origin"));
        response->setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        response->setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
}

// Generic helper for building responses with status and body
HTTPResponse buildResponse(const std::string& status, const std::string& body, const HTTPRequest* request) {
    HTTPResponse response(status);
    addDefaultHeaders(&response, request);
    response.setBody(body);
    return response;
}

HTTPResponse responseOK(const HTTPRequest* request) { return buildResponse("200 OK", "200 OK", request); }

HTTPResponse responseCreated(const HTTPRequest* request) {
    return buildResponse("201 Created", "201 Created", request);
}

HTTPResponse responseAccepted(const HTTPRequest* request) {
    return buildResponse("202 Accepted", "202 Accepted", request);
}

HTTPResponse responseNonAuthoritative(const HTTPRequest* request) {
    return buildResponse("203 Non-Authoritative Information", "203 Non-Authoritative Information", request);
}

HTTPResponse responseNoContent(const HTTPRequest* request) {
    return buildResponse("204 No Content", "204 No Content", request);
}

HTTPResponse responseResetContent(const HTTPRequest* request) {
    return buildResponse("205 Reset Content", "205 Reset Content", request);
}

HTTPResponse responsePartialContent(const HTTPRequest* request) {
    return buildResponse("206 Partial Content", "206 Partial Content", request);
}

HTTPResponse responseBadRequest(const HTTPRequest* request) {
    return buildResponse("400 Bad Request", "400 Bad Request", request);
}

HTTPResponse responseAuthRequired(const HTTPRequest* request) {
    return buildResponse("401 Unauthorized", "401 Unauthorized", request);
}

HTTPResponse responseUnauthorizedBasicAuth(const std::string& realm, const HTTPRequest* request) {
    HTTPResponse response("401 Unauthorized");
    addDefaultHeaders(&response, request);
    response.setHeader("WWW-Authenticate", "Basic realm=\"" + realm + "\"");
    response.setBody("401 Unauthorized - Authentication Required");
    return response;
}

HTTPResponse responseForbidden(const HTTPRequest* request) {
    return buildResponse("403 Forbidden", "403 Forbidden", request);
}

HTTPResponse responseNotFound(const HTTPRequest* request) {
    return buildResponse("404 Not Found", "404 Not Found", request);
}

HTTPResponse responseMethodNotAllowed(const HTTPRequest* request) {
    return buildResponse("405 Method Not Allowed", "405 Method Not Allowed", request);
}

HTTPResponse responseMethodNotAllowed(const HTTPRequest* request, std::string_view allowMethods) {
    HTTPResponse response = buildResponse("405 Method Not Allowed", "405 Method Not Allowed", request);
    if (!allowMethods.empty()) {
        response.setHeader("Allow", std::string(allowMethods));
    }
    return response;
}

HTTPResponse responseConflict(const HTTPRequest* request) {
    return buildResponse("409 Conflict", "409 Conflict", request);
}

HTTPResponse responseInternalServerError(const HTTPRequest* request) {
    return buildResponse("500 Internal Server Error", "500 Internal Server Error", request);
}

}  // namespace geruest
