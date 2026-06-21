#include <gtest/gtest.h>

#include "data/HTTPRequest.hpp"
#include "data/ServerData.hpp"
#include "handler/StaticFileResolver.hpp"

using namespace geruest;

namespace {

HTTPRequest makeRequest(const std::string& acceptLanguage = {}) {
    std::string headers = "GET / HTTP/1.1\r\nHost: example.com\r\n";
    if (!acceptLanguage.empty()) {
        headers += "Accept-Language: " + acceptLanguage + "\r\n";
    }
    headers += "\r\n";
    return HTTPRequest(HttpHeadersOnlyTag{}, headers, "127.0.0.1", "/site/root");
}

}  // namespace

TEST(StaticFileResolver, RootPathWithLanguages) {
    ServerData sd;
    sd.setRoot("/site/root");
    sd.setAvailableLanguages({"en", "de"});

    StaticFileResolver resolver(sd);
    std::string path = "/";
    const std::string result = resolver.buildPath(path, StaticFileResolver::getExtension(path), makeRequest("de"));

    EXPECT_EQ(result, "/site/root/html/de/index.html");
}

TEST(StaticFileResolver, AssetsPathDirect) {
    ServerData sd;
    sd.setRoot("/site/root");

    StaticFileResolver resolver(sd);
    std::string path = "/assets/images/foo.png";
    const std::string result = resolver.buildPath(path, StaticFileResolver::getExtension(path), makeRequest());

    EXPECT_EQ(result, "/site/root/assets/images/foo.png");
}

TEST(StaticFileResolver, BlocksPathTraversal) {
    ServerData sd;
    sd.setRoot("/site/root");

    StaticFileResolver resolver(sd);
    std::string path = "/../../etc/passwd";
    const std::string result = resolver.buildPath(path, StaticFileResolver::getExtension(path), makeRequest());

    EXPECT_TRUE(result.empty());
}

TEST(StaticFileResolver, UnknownExtensionReturnsEmpty) {
    ServerData sd;
    sd.setRoot("/site/root");

    StaticFileResolver resolver(sd);
    std::string path = "/file.unknownext";
    const std::string result = resolver.buildPath(path, StaticFileResolver::getExtension(path), makeRequest());

    EXPECT_TRUE(result.empty());
}

TEST(StaticFileResolver, GetExtensionDefaultsToHtml) {
    EXPECT_EQ(StaticFileResolver::getExtension("/page"), "html");
    EXPECT_EQ(StaticFileResolver::getExtension("/page.html"), "html");
}

TEST(StaticFileResolver, GetContentTypeKnownExtension) {
    EXPECT_EQ(StaticFileResolver::getContentType("css"), "text/css");
    EXPECT_EQ(StaticFileResolver::getContentType("unknown"), "application/octet-stream");
}
