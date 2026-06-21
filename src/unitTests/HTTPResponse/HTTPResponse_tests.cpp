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
#include "../../data/MethodNotAllowed.hpp"

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
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "Origin: https://example.com\r\n"
                            "\r\n";
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");

    HTTPResponse response = responseOK(&request);
    std::string responseStr = response.toString();

    // CORS is applied centrally via enableCors(), not in generic response helpers.
    EXPECT_EQ(responseStr.find("Access-Control-Allow-Origin"), std::string::npos);
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

TEST(HTTPResponseTest, MethodNotAllowedAllowHeader) {
    HTTPRequest req("GET / HTTP/1.1\r\nHost: x\r\n\r\n", "127.0.0.1", "/");
    HTTPResponse r = responseMethodNotAllowed(&req, "GET, HEAD");
    const std::string s = r.toString();
    EXPECT_NE(s.find("HTTP/1.1 405 Method Not Allowed"), std::string::npos);
    EXPECT_NE(s.find("Allow: GET, HEAD"), std::string::npos);
}

TEST(HTTPResponseTest, MethodNotAllowedExceptionType) {
    try {
        throw method_not_allowed("GET, POST");
    } catch (const method_not_allowed& e) {
        EXPECT_EQ(e.allowMethods(), "GET, POST");
        EXPECT_NE(std::string(e.what()).find("405"), std::string::npos);
    }
}

TEST(HTTPResponseTest, SerializeToMatchesToString) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Server", "Test/1");
    response.setBody("{\"x\":1}");
    response.addHeader("Set-Cookie", "a=1");
    response.addHeader("Set-Cookie", "b=2");

    const std::string expected = response.toString();
    std::string       scratch;
    response.serializeTo(scratch);
    EXPECT_EQ(scratch, expected);
}

TEST(HTTPResponseTest, SetHeaderRemovesAllSameKey) {
    HTTPResponse response("200 OK");
    response.addHeader("X-Test", "first");
    response.addHeader("X-Test", "second");
    response.setHeader("X-Test", "third");
    std::string s = response.toString();
    EXPECT_EQ(s.find("X-Test: first"), std::string::npos);
    EXPECT_EQ(s.find("X-Test: second"), std::string::npos);
    EXPECT_NE(s.find("X-Test: third"), std::string::npos);
}