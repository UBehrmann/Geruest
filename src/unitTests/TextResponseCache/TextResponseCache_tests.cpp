#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

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

}  // namespace

TEST_F(TextResponseCacheTest, HitWhileMtimeUnchanged) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", false, maxBytes, maxBytes);
    const auto hit = cache.lookup(key, path_.string(), false, maxBytes, maxBytes);

    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(*hit, "cached-body");
}

TEST_F(TextResponseCacheTest, MissAfterFileEdit) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", false, maxBytes, maxBytes);
    ASSERT_NE(cache.lookup(key, path_.string(), false, maxBytes, maxBytes), nullptr);

    std::ofstream(path_, std::ios::app) << "\nbody { color: blue; }";
    EXPECT_EQ(cache.lookup(key, path_.string(), false, maxBytes, maxBytes), nullptr);
}

TEST_F(TextResponseCacheTest, DevModeBypassesCache) {
    TextResponseCache cache;
    const std::string key = TextResponseCache::makeKey("text/css", path_.string());
    constexpr size_t maxBytes = 1024 * 1024;

    cache.store(key, path_.string(), "cached-body", false, maxBytes, maxBytes);
    EXPECT_EQ(cache.lookup(key, path_.string(), true, maxBytes, maxBytes), nullptr);
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

    a.store(key, path, "from-a", false, maxBytes, maxBytes);
    EXPECT_NE(a.lookup(key, path, false, maxBytes, maxBytes), nullptr);
    EXPECT_EQ(b.lookup(key, path, false, maxBytes, maxBytes), nullptr);

    std::filesystem::remove(path);
}
