/**
 * @file HTTPResponse.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This function is used to build HTTP headers.
 */

#include "HTTPResponse.hpp"
#include "HTTPRequest.hpp"

namespace geruest {

HTTPResponse::HTTPResponse(const std::string& statusCode) : status(statusCode) {
    // Default headers
    headers.insert({"Content-Type", "text/html"});
    headers.insert({"Connection", "Keep-Alive"});
    headers.insert({"Keep-Alive", "timeout=5, max=100"});
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value) {

    if (key == "Content-Length") {
        return; // Do not set Content-Length here, it will be handled in setBody
    }

    // Remove any existing headers with the same key, then add the new one
    headers.erase(key);
    headers.insert({key, value});
}

void HTTPResponse::addHeader(const std::string& key, const std::string& value) {

    if (key == "Content-Length") {
        return; // Do not set Content-Length here, it will be handled in setBody
    }

    // Add header to the multimap, allowing multiple headers with same key
    headers.insert({key, value});
}

void HTTPResponse::setBody(const std::string& responseBody) {
    body = responseBody;
    // Remove existing Content-Length headers and add new one
    headers.erase("Content-Length");
    headers.insert({"Content-Length", std::to_string(body.size())});
}

std::string HTTPResponse::toString() const {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n";

    // Add all headers from the multimap
    for (const auto& header : headers) {
        response << header.first << ": " << header.second << "\r\n";
    }

    // Check body size and add Content-Length if body is not empty
    if (!body.empty()) {
        response << "Content-Length: " << body.size() << "\r\n";

        // add a blank line to separate headers from the body
        response << "\r\n";

        // Append the body to the response
        response << body;

    } else {
        response << "Content-Length: 0\r\n";
    }

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
HTTPResponse buildResponse(const std::string& status, const std::string& body, HTTPRequest* request) {
    HTTPResponse response(status);
    addDefaultHeaders(&response, request);
    response.setBody(body);
    return response;
}

HTTPResponse responseOK(HTTPRequest* request){
    return buildResponse("200 OK", "200 OK", request);
}

HTTPResponse responseCreated(HTTPRequest* request){
    return buildResponse("201 Created", "201 Created", request);
}

HTTPResponse responseAccepted(HTTPRequest* request){
    return buildResponse("202 Accepted", "202 Accepted", request);
}

HTTPResponse responseNonAuthoritative(HTTPRequest* request){
    return buildResponse("203 Non-Authoritative Information", "203 Non-Authoritative Information", request);
}

HTTPResponse responseNoContent(HTTPRequest* request){
    return buildResponse("204 No Content", "204 No Content", request);
}

HTTPResponse responseResetContent(HTTPRequest* request){
    return buildResponse("205 Reset Content", "205 Reset Content", request);
}

HTTPResponse responsePartialContent(HTTPRequest* request){
    return buildResponse("206 Partial Content", "206 Partial Content", request);
}

HTTPResponse responseBadRequest(HTTPRequest* request){
    return buildResponse("400 Bad Request", "400 Bad Request", request);
}

HTTPResponse responseAuthRequired(HTTPRequest* request){
    HTTPResponse response("401 Unauthorized");
    addDefaultHeaders(&response, request);
    response.setHeader("WWW-Authenticate", "Basic realm=\"Restricted Area\"");
    response.setBody("401 Unauthorized");
    return response;
}

HTTPResponse responseForbidden(HTTPRequest* request){
    return buildResponse("403 Forbidden", "403 Forbidden", request);
}

HTTPResponse responseNotFound(HTTPRequest* request){
    return buildResponse("404 Not Found", "404 Not Found", request);
}

HTTPResponse responseMethodNotAllowed(HTTPRequest* request){
    return buildResponse("405 Method Not Allowed", "405 Method Not Allowed", request);
}

HTTPResponse responseConflict(HTTPRequest* request){
    return buildResponse("409 Conflict", "409 Conflict", request);
}

HTTPResponse responseInternalServerError(HTTPRequest* request){
    return buildResponse("500 Internal Server Error", "500 Internal Server Error", request);
}



}  // namespace geruest
