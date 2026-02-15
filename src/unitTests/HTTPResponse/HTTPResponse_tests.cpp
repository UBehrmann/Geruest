/**
 * @file HTTPResponse_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the HTTPResponse class
 */

#include <gtest/gtest.h>
#include <string>
#include "../../data/HTTPResponse.hpp"
#include "../../data/HTTPRequest.hpp"

using namespace geruest;

TEST(HTTPResponseTest, ResponseCreation) {
    HTTPResponse response("200 OK");
    std::string responseStr = response.toString();
    
    // Should contain status line
    EXPECT_NE(responseStr.find("HTTP/1.1 200 OK"), std::string::npos);
}

TEST(HTTPResponseTest, SetHeader) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "text/html");
    response.setHeader("Content-Length", "13");
    
    std::string responseStr = response.toString();
    
    EXPECT_NE(responseStr.find("Content-Type: text/html"), std::string::npos);
    EXPECT_NE(responseStr.find("Content-Length: 13"), std::string::npos);
}

TEST(HTTPResponseTest, SetBody) {
    HTTPResponse response("200 OK");
    response.setBody("Hello, World!");
    
    std::string responseStr = response.toString();
    
    // Should contain body
    EXPECT_NE(responseStr.find("Hello, World!"), std::string::npos);
    // Should automatically set Content-Length
    EXPECT_NE(responseStr.find("Content-Length: 13"), std::string::npos);
}

TEST(HTTPResponseTest, AddHeader) {
    HTTPResponse response("200 OK");
    response.addHeader("Set-Cookie", "session=abc123");
    response.addHeader("Set-Cookie", "user=john");
    
    std::string responseStr = response.toString();
    
    // Should contain both Set-Cookie headers
    EXPECT_NE(responseStr.find("Set-Cookie: session=abc123"), std::string::npos);
    EXPECT_NE(responseStr.find("Set-Cookie: user=john"), std::string::npos);
}

TEST(HTTPResponseTest, CompleteResponse) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Server", "Geruest/1.0");
    response.setBody("{\"status\":\"success\"}");
    
    std::string responseStr = response.toString();
    std::string body = "{\"status\":\"success\"}";
    std::string expectedLength = "Content-Length: " + std::to_string(body.length());
    
    // Should have proper HTTP response format
    EXPECT_NE(responseStr.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(responseStr.find("Content-Type: application/json"), std::string::npos);
    EXPECT_NE(responseStr.find("Server: Geruest/1.0"), std::string::npos);
    EXPECT_NE(responseStr.find(expectedLength), std::string::npos);
    EXPECT_NE(responseStr.find(body), std::string::npos);
    
    // Should have proper HTTP structure (headers followed by blank line, then body)
    EXPECT_NE(responseStr.find("\r\n\r\n"), std::string::npos);
}

TEST(HTTPResponseTest, PredefinedResponseFunctions) {
    // Test some of the predefined response functions
    HTTPResponse okResponse = responseOK();
    EXPECT_NE(okResponse.toString().find("HTTP/1.1 200 OK"), std::string::npos);
    
    HTTPResponse notFoundResponse = responseNotFound();
    EXPECT_NE(notFoundResponse.toString().find("HTTP/1.1 404 Not Found"), std::string::npos);
    
    HTTPResponse badRequestResponse = responseBadRequest();
    EXPECT_NE(badRequestResponse.toString().find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST(HTTPResponseTest, ResponseWithRequestContext) {
    // Create a mock request
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "Origin: https://example.com\r\n"
                            "\r\n";
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    // Test response with request context
    HTTPResponse response = responseOK(&request);
    std::string responseStr = response.toString();
    
    // Should contain CORS headers when Origin is present
    EXPECT_NE(responseStr.find("Access-Control-Allow-Origin: https://example.com"), std::string::npos);
}

TEST(HTTPResponseTest, BuildHeaderFunctions) {
    // Test legacy build functions
    std::string badRequestHeader = buildBadRequestHeader();
    EXPECT_NE(badRequestHeader.find("HTTP/1.1 400 Bad Request"), std::string::npos);
    
    std::string notFoundHeader = buildNotFoundHeader();
    EXPECT_NE(notFoundHeader.find("HTTP/1.1 404 Not Found"), std::string::npos);
    
    std::string authHeader = buildAuthHeader();
    EXPECT_NE(authHeader.find("HTTP/1.1 401 Unauthorized"), std::string::npos);
    EXPECT_NE(authHeader.find("WWW-Authenticate"), std::string::npos);
}