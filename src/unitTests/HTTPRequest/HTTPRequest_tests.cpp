/**
 * @file HTTPRequest_tests.cpp
 * @created 2024-09-29
 * @updated 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the HTTPRequest class using Google Test
 */

#include <gtest/gtest.h>
#include <string>
#include "../../data/HTTPRequest.hpp"

using namespace geruest;

TEST(HTTPRequestTest, BasicParsing) {
    std::string rawRequest = "GET /test HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "User-Agent: Test/1.0\r\n"
                            "Content-Length: 4\r\n"
                            "\r\n"
                            "test";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    EXPECT_EQ(request.getMethod(), "GET");
    EXPECT_EQ(request.getPathString(), "/test");
    EXPECT_TRUE(request.hasHeader("host"));
    EXPECT_EQ(request.getHeader("host"), "example.com");
    EXPECT_EQ(request.getHeader("user-agent"), "Test/1.0");
    EXPECT_EQ(request.getBody(), "test");
}

TEST(HTTPRequestTest, PostWithBody) {
    std::string rawRequest = "POST /api/users HTTP/1.1\r\n"
                            "Host: api.example.com\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: 25\r\n"
                            "\r\n"
                            "{\"name\":\"John Doe\"}";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    EXPECT_EQ(request.getMethod(), "POST");
    EXPECT_EQ(request.getPathString(), "/api/users");
    EXPECT_EQ(request.getHeader("content-type"), "application/json");
    EXPECT_EQ(request.getBody(), "{\"name\":\"John Doe\"}");
}

TEST(HTTPRequestTest, QueryParameters) {
    std::string rawRequest = "GET /search?q=test&limit=10 HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "\r\n";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    EXPECT_EQ(request.getMethod(), "GET");
    EXPECT_EQ(request.getPathString(), "/search");
    EXPECT_TRUE(request.hasParam("q"));
    EXPECT_EQ(request.getParam("q"), "test");
    EXPECT_TRUE(request.hasParam("limit"));
    EXPECT_EQ(request.getParam("limit"), "10");
}

TEST(HTTPRequestTest, URLDecode) {
    std::string encoded = "Hello%20World%21";
    std::string decoded = urlDecode(encoded);
    EXPECT_EQ(decoded, "Hello World!");
}

TEST(HTTPRequestTest, EmptyBody) {
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "\r\n";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    EXPECT_EQ(request.getMethod(), "GET");
    EXPECT_EQ(request.getPathString(), "/");
    EXPECT_TRUE(request.getBody().empty());
}

TEST(HTTPRequestTest, CaseInsensitiveHeaders) {
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "HOST: example.com\r\n"
                            "USER-AGENT: Test/1.0\r\n"
                            "\r\n";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    EXPECT_TRUE(request.hasHeader("host"));
    EXPECT_TRUE(request.hasHeader("HOST"));
    EXPECT_TRUE(request.hasHeader("Host"));
    EXPECT_EQ(request.getHeader("host"), "example.com");
}

TEST(HTTPRequestTest, HttpExpect100ContinueMatcher) {
    EXPECT_TRUE(httpExpectIs100Continue("100-continue"));
    EXPECT_TRUE(httpExpectIs100Continue("100-Continue"));
    EXPECT_TRUE(httpExpectIs100Continue("  100-continue  "));
    EXPECT_TRUE(httpExpectIs100Continue("100-continue, continue"));
    EXPECT_FALSE(httpExpectIs100Continue("continue"));
    EXPECT_FALSE(httpExpectIs100Continue(""));
    EXPECT_FALSE(httpExpectIs100Continue("100"));
}
