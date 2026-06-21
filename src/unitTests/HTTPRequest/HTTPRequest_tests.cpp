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

TEST(HTTPRequestTest, JsonBodyCommaInsideStringValue) {
    const std::string body = R"({"user_id":"1","key":"k","author":"Alpha, Beta","title":"T","genre":"fiction"})";
    std::string rawRequest = "PUT /v1/books HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: " + std::to_string(body.size()) + "\r\n"
                            "\r\n" + body;

    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");

    EXPECT_TRUE(request.hasParam("author"));
    EXPECT_EQ(request.getParam("author"), "Alpha, Beta");
    EXPECT_EQ(request.getParam("title"), "T");
    EXPECT_EQ(request.getParam("genre"), "fiction");
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

TEST(HTTPRequestTest, HttpShouldCloseAfterResponse) {
    EXPECT_TRUE(httpShouldCloseAfterResponse("GET / HTTP/1.1", "close"));
    EXPECT_TRUE(httpShouldCloseAfterResponse("GET / HTTP/1.1", "Close"));
    EXPECT_TRUE(httpShouldCloseAfterResponse("GET / HTTP/1.1", "keep-alive, close"));
    EXPECT_FALSE(httpShouldCloseAfterResponse("GET / HTTP/1.1", "keep-alive"));
    EXPECT_FALSE(httpShouldCloseAfterResponse("GET / HTTP/1.1", ""));
    EXPECT_TRUE(httpShouldCloseAfterResponse("GET / HTTP/1.0", ""));
    EXPECT_FALSE(httpShouldCloseAfterResponse("GET / HTTP/1.0", "keep-alive"));
}

TEST(HTTPRequestTest, SplitHttpHeadersCrlf) {
    const std::string raw = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\nbody";
    const auto split = splitHttpHeaders(raw);
    ASSERT_TRUE(split.has_value());
    EXPECT_EQ(split->delimiterLength, 4u);
    EXPECT_EQ(split->headerSectionEnd, raw.find("body"));
    EXPECT_EQ(raw.substr(split->headerSectionEnd), "body");
}

TEST(HTTPRequestTest, SplitHttpHeadersLf) {
    const std::string raw = "GET / HTTP/1.1\nHost: example.com\n\nbody";
    const auto split = splitHttpHeaders(raw);
    ASSERT_TRUE(split.has_value());
    EXPECT_EQ(split->delimiterLength, 2u);
    EXPECT_EQ(raw.substr(split->headerSectionEnd), "body");
}

TEST(HTTPRequestTest, SplitHttpHeadersCr) {
    const std::string raw = "GET / HTTP/1.1\rHost: example.com\r\rbody";
    const auto split = splitHttpHeaders(raw);
    ASSERT_TRUE(split.has_value());
    EXPECT_EQ(split->delimiterLength, 2u);
    EXPECT_EQ(raw.substr(split->headerSectionEnd), "body");
}

TEST(HTTPRequestTest, SplitHttpHeadersNotFound) {
    EXPECT_FALSE(splitHttpHeaders("GET / HTTP/1.1\r\nHost: x").has_value());
}

TEST(HTTPRequestTest, ParseHeaderPreflight) {
    const std::string raw =
        "POST / HTTP/1.1\r\n"
        "Expect: 100-continue\r\n"
        "Content-Length: 42\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    const auto split = splitHttpHeaders(raw);
    ASSERT_TRUE(split.has_value());
    const HeaderPreflight preflight = parseHeaderPreflight(std::string_view(raw.data(), split->headerSectionEnd));
    EXPECT_EQ(preflight.expect, "100-continue");
    EXPECT_EQ(preflight.contentLength, "42");
    EXPECT_EQ(preflight.transferEncoding, "chunked");
}

TEST(HTTPRequestTest, HttpConnectionHeaderHasChunkedToken) {
    EXPECT_TRUE(httpConnectionHeaderHasToken("chunked", "chunked"));
    EXPECT_TRUE(httpConnectionHeaderHasToken("gzip, chunked", "chunked"));
    EXPECT_FALSE(httpConnectionHeaderHasToken("gzip", "chunked"));
}

TEST(HTTPRequestTest, ParseContentLengthBytes) {
    size_t bytes = 0;
    EXPECT_TRUE(parseContentLengthBytes("0", &bytes));
    EXPECT_EQ(bytes, 0u);
    EXPECT_TRUE(parseContentLengthBytes("12345", &bytes));
    EXPECT_EQ(bytes, 12345u);
    EXPECT_FALSE(parseContentLengthBytes("", &bytes));
    EXPECT_FALSE(parseContentLengthBytes("12x", &bytes));
}

TEST(HTTPRequestTest, FindChunkedBodyEndZeroChunk) {
    const std::string raw = "POST / HTTP/1.1\r\nHost: x\r\n\r\n0\r\n\r\n";
    const auto split = splitHttpHeaders(raw);
    ASSERT_TRUE(split.has_value());
    const size_t end = findChunkedBodyEnd(raw, split->headerSectionEnd);
    EXPECT_EQ(end, raw.size());
}

TEST(HTTPRequestTest, FindChunkedBodyEndOneChunk) {
    const std::string raw = "POST / HTTP/1.1\r\nHost: x\r\n\r\n5\r\nhello\r\n0\r\n\r\n";
    const auto split = splitHttpHeaders(raw);
    ASSERT_TRUE(split.has_value());
    const size_t end = findChunkedBodyEnd(raw, split->headerSectionEnd);
    EXPECT_EQ(end, raw.size());
}
