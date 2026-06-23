#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "data/DevAssetCache.hpp"

using namespace geruest;

TEST(DevAssetCacheTest, MergedAssetPutGetHas) {
    DevAssetCache cache;
    EXPECT_FALSE(cache.hasMergedAsset("/assets/css/page.css"));
    EXPECT_TRUE(cache.getMergedAsset("/assets/css/page.css").empty());

    cache.putMergedAsset("/assets/css/page.css", "body { color: red; }");
    EXPECT_TRUE(cache.hasMergedAsset("/assets/css/page.css"));
    EXPECT_EQ(cache.getMergedAsset("/assets/css/page.css"), "body { color: red; }");
}

TEST(DevAssetCacheTest, WebPPutGetHas) {
    DevAssetCache cache;
    auto data = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{0x52, 0x49, 0x46, 0x46});

    EXPECT_FALSE(cache.hasWebP("/tmp/site/assets/images/a.webp"));
    EXPECT_EQ(cache.getWebP("/tmp/site/assets/images/a.webp"), nullptr);

    cache.putWebP("/tmp/site/assets/images/a.webp", data);
    EXPECT_TRUE(cache.hasWebP("/tmp/site/assets/images/a.webp"));
    ASSERT_NE(cache.getWebP("/tmp/site/assets/images/a.webp"), nullptr);
    EXPECT_EQ(*cache.getWebP("/tmp/site/assets/images/a.webp"), *data);
}

TEST(DevAssetCacheTest, WebPLruEviction) {
    DevAssetCache cache;
    cache.setMaxWebPBytes(10);

    auto a = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1, 2, 3, 4});
    auto b = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{5, 6, 7, 8});
    auto c = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{9, 10, 11, 12});

    cache.putWebP("/a.webp", a);
    cache.putWebP("/b.webp", b);
    cache.putWebP("/c.webp", c);

    EXPECT_FALSE(cache.hasWebP("/a.webp"));
    EXPECT_TRUE(cache.hasWebP("/b.webp"));
    EXPECT_TRUE(cache.hasWebP("/c.webp"));
}

TEST(DevAssetCacheTest, ClearResetsBothMaps) {
    DevAssetCache cache;
    cache.putMergedAsset("/assets/js/app.js", "console.log(1);");
    cache.putWebP("/img.webp", std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1}));

    cache.clear();

    EXPECT_FALSE(cache.hasMergedAsset("/assets/js/app.js"));
    EXPECT_FALSE(cache.hasWebP("/img.webp"));
}
