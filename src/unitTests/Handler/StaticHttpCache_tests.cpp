#include <gtest/gtest.h>

#include "data/HTTPRequest.hpp"
#include "data/ServerData.hpp"
#include "handler/StaticHttpCache.hpp"

namespace {

geruest::HTTPRequest makeGet(const std::string& raw) {
    return geruest::HTTPRequest(std::move(raw), "127.0.0.1", "/site");
}

}  // namespace

TEST(StaticHttpCache, DevModeUsesNoStore) {
    geruest::ServerData sd;
    sd.enableDevMode();
    const geruest::StaticCacheHeaders headers =
        geruest::resolveStaticCacheHeaders(sd, "text/css", "css", false);
    EXPECT_EQ(headers.cacheControl, "no-store");
}

TEST(StaticHttpCache, HtmlDefaultMustRevalidate) {
    geruest::ServerData sd;
    const geruest::StaticCacheHeaders headers =
        geruest::resolveStaticCacheHeaders(sd, "text/html", "html", false);
    EXPECT_EQ(headers.cacheControl, "public, max-age=0, must-revalidate");
}

TEST(StaticHttpCache, AssetsUseLongImmutableCache) {
    geruest::ServerData sd;
    const geruest::StaticCacheHeaders headers =
        geruest::resolveStaticCacheHeaders(sd, "text/javascript", "js", false);
    EXPECT_EQ(headers.cacheControl, "public, max-age=31536000, immutable");
}

TEST(StaticHttpCache, MatchesIfNoneMatch) {
    geruest::StaticCacheHeaders headers;
    headers.etag = "\"abc123\"";
    headers.cacheControl = "public, max-age=0, must-revalidate";

    geruest::HTTPRequest match =
        makeGet("GET / HTTP/1.1\r\nHost: x\r\nIf-None-Match: \"abc123\"\r\n\r\n");
    geruest::HTTPRequest miss =
        makeGet("GET / HTTP/1.1\r\nHost: x\r\nIf-None-Match: \"other\"\r\n\r\n");

    EXPECT_TRUE(geruest::matchesNotModified(match, headers));
    EXPECT_FALSE(geruest::matchesNotModified(miss, headers));
}

TEST(StaticHttpCache, EtagFromBodyStable) {
    EXPECT_EQ(geruest::makeEtagFromBody("hello"), geruest::makeEtagFromBody("hello"));
    EXPECT_NE(geruest::makeEtagFromBody("hello"), geruest::makeEtagFromBody("world"));
}
