#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "data/HTTPRequest.hpp"
#include "data/TextResponseCache.hpp"

using namespace geruest;

namespace {

class TextResponseCacheTest : public ::testing::Test {
   protected:
    void SetUp() override {
        path_ = std::filesystem::temp_directory_path() / "geruest_text_cache_test.css";
        std::ofstream(path_) << "body { color: red; }";
    }

    void TearDown() override { std::filesystem::remove(path_); }

    std::filesystem::path path_;
};

HTTPRequest makeRequestWithEtag(const std::string& etag) {
    return HTTPRequest("GET / HTTP/1.1\r\nHost: x\r\nIf-None-Match: " + etag + "\r\n\r\n", "127.0.0.1", "/site");
}

}  // namespace

TEST_F(TextResponseCacheTest, HitWhileMtimeUnchanged) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", "\"etag1\"", "Mon, 01 Jan 2024 00:00:00 GMT", false, maxBytes,
                maxBytes);
    const TextCacheLookup hit = cache.lookup(key, path_.string(), nullptr, false, maxBytes, maxBytes);

    ASSERT_TRUE(hit.payload != nullptr);
    EXPECT_EQ(*hit.payload, "cached-body");
}

TEST_F(TextResponseCacheTest, NotModifiedWhenEtagMatches) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", "\"etag1\"", "Mon, 01 Jan 2024 00:00:00 GMT", false, maxBytes,
                maxBytes);
    HTTPRequest request = makeRequestWithEtag("\"etag1\"");
    const TextCacheLookup hit = cache.lookup(key, path_.string(), &request, false, maxBytes, maxBytes);

    EXPECT_TRUE(hit.notModified);
    EXPECT_EQ(hit.payload, nullptr);
}

TEST_F(TextResponseCacheTest, MissAfterFileEdit) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", "\"etag1\"", "", false, maxBytes, maxBytes);
    ASSERT_NE(cache.lookup(key, path_.string(), nullptr, false, maxBytes, maxBytes).payload, nullptr);

    std::ofstream(path_, std::ios::app) << "\nbody { color: blue; }";
    EXPECT_EQ(cache.lookup(key, path_.string(), nullptr, false, maxBytes, maxBytes).payload, nullptr);
}

TEST_F(TextResponseCacheTest, DevModeBypassesCache) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", "\"etag1\"", "", false, maxBytes, maxBytes);
    EXPECT_EQ(cache.lookup(key, path_.string(), nullptr, true, maxBytes, maxBytes).payload, nullptr);
}

TEST(TextResponseCacheIsolation, SeparateCacheInstances) {
    TextResponseCache a;
    TextResponseCache b;
    const std::string path = (std::filesystem::temp_directory_path() / "geruest_text_cache_isolation.css").string();
    {
        std::ofstream out(path);
        out << "body { margin: 0; }";
    }

    const std::string key = TextResponseCache::makeKey("text/css", path);
    constexpr size_t maxBytes = 1024 * 1024;

    a.store(key, path, "from-a", "\"etag1\"", "", false, maxBytes, maxBytes);
    EXPECT_NE(a.lookup(key, path, nullptr, false, maxBytes, maxBytes).payload, nullptr);
    EXPECT_EQ(b.lookup(key, path, nullptr, false, maxBytes, maxBytes).payload, nullptr);

    std::filesystem::remove(path);
}
