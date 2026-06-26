#include <gtest/gtest.h>

#include "builders/CacheBustQuery.hpp"

TEST(CacheBustQuery, AppendsQueryParam) {
    EXPECT_EQ(geruest::appendCacheBustParam("/app.js", "abc"), "/app.js?v=abc");
    EXPECT_EQ(geruest::appendCacheBustParam("/app.js?x=1", "abc"), "/app.js?x=1&v=abc");
}

TEST(CacheBustQuery, SkipsExternalUrls) {
    std::string html = "<script src=\"https://cdn.example/lib.js\"></script>";
    geruest::appendCacheBustToHtml(html, "abc");
    EXPECT_EQ(html.find("?v=abc"), std::string::npos);
}

TEST(CacheBustQuery, BustsLocalAssets) {
    std::string html =
        "<link rel=\"stylesheet\" href=\"/style.css\">"
        "<script src=\"/main.js\"></script>"
        "<img src=\"/logo.png\">";
    geruest::appendCacheBustToHtml(html, "tok");
    EXPECT_NE(html.find("/style.css?v=tok"), std::string::npos);
    EXPECT_NE(html.find("/main.js?v=tok"), std::string::npos);
    EXPECT_NE(html.find("/logo.png?v=tok"), std::string::npos);
}

TEST(CacheBustQuery, SkipsWhenAlreadyPresent) {
    std::string html = "<script src=\"/main.js?v=existing\"></script>";
    geruest::appendCacheBustToHtml(html, "tok");
    EXPECT_EQ(html.find("&v=tok"), std::string::npos);
    EXPECT_NE(html.find("?v=existing"), std::string::npos);
}
