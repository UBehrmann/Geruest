#include <gtest/gtest.h>

#include "data/CorsConfig.hpp"
#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"

using namespace geruest;

namespace {

HTTPRequest makeRequest(const std::string& method, const std::string& path, const std::string& extraHeaders = {}) {
    std::string raw = method + " " + path + " HTTP/1.1\r\nHost: example.com\r\n" + extraHeaders + "\r\n";
    return HTTPRequest(raw, "127.0.0.1", "/root");
}

CorsConfig enabledCors() {
    return CorsConfig::fromOptions({.origins = {"https://app.example.com"}, .paths = {"/v1/*", "/api/*"}});
}

}  // namespace

TEST(CorsConfigTest, DisabledWhenOptionsEmpty) {
    EXPECT_FALSE(CorsConfig::fromOptions({.origins = {}, .paths = {"/v1/*"}}).isEnabled());
    EXPECT_FALSE(CorsConfig::fromOptions({.origins = {"*"}, .paths = {}}).isEnabled());
}

TEST(CorsConfigTest, PathMatching) {
    const CorsConfig cors = enabledCors();
    EXPECT_TRUE(cors.matchesPath("/v1/users"));
    EXPECT_TRUE(cors.matchesPath("/api/contact"));
    EXPECT_FALSE(cors.matchesPath("/static/app.js"));
}

TEST(CorsConfigTest, OriginAllowlist) {
    const CorsConfig cors = enabledCors();
    EXPECT_EQ(cors.resolveOrigin("https://app.example.com"), "https://app.example.com");
    EXPECT_FALSE(cors.resolveOrigin("https://evil.example.com").has_value());
    EXPECT_FALSE(cors.resolveOrigin("").has_value());
}

TEST(CorsConfigTest, WildcardOrigin) {
    const CorsConfig cors = CorsConfig::fromOptions({.origins = {"*"}, .paths = {"/v1/*"}});
    EXPECT_EQ(cors.resolveOrigin("https://any.example.com"), "*");
}

TEST(CorsConfigTest, ApplyHeadersOnMatchingRoute) {
    const CorsConfig cors = enabledCors();
    HTTPRequest request = makeRequest("GET", "/v1/items", "Origin: https://app.example.com\r\n");
    HTTPResponse response("200 OK");
    applyCorsHeaders(response, cors, &request);

    const std::string serialized = response.toString();
    EXPECT_NE(serialized.find("Access-Control-Allow-Origin: https://app.example.com"), std::string::npos);
    EXPECT_NE(serialized.find("Access-Control-Allow-Methods:"), std::string::npos);
    EXPECT_NE(serialized.find("Vary: Origin"), std::string::npos);
}

TEST(CorsConfigTest, PreflightEchoesRequestedHeaders) {
    const CorsConfig cors = enabledCors();
    HTTPRequest request = makeRequest(
        "OPTIONS", "/v1/items",
        "Origin: https://app.example.com\r\nAccess-Control-Request-Headers: X-Custom-Header\r\n");
    HTTPResponse response = responseNoContent(&request);
    applyCorsHeaders(response, cors, &request, true);

    const std::string serialized = response.toString();
    EXPECT_NE(serialized.find("Access-Control-Allow-Headers: X-Custom-Header"), std::string::npos);
    EXPECT_NE(serialized.find("Access-Control-Max-Age: 86400"), std::string::npos);
}

TEST(CorsConfigTest, SkipsDisallowedOrigin) {
    const CorsConfig cors = enabledCors();
    HTTPRequest request = makeRequest("GET", "/v1/items", "Origin: https://evil.example.com\r\n");
    HTTPResponse response("200 OK");
    applyCorsHeaders(response, cors, &request);

    EXPECT_EQ(response.toString().find("Access-Control-Allow-Origin"), std::string::npos);
}
