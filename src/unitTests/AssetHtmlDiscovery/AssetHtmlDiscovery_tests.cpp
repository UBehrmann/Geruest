/**
 * @file AssetHtmlDiscovery_tests.cpp
 * @brief Unit tests for HTML asset reference discovery (no merge I/O).
 */

#include <gtest/gtest.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "../../builders/AssetHtmlDiscovery.hpp"

using namespace geruest;

TEST(AssetHtmlDiscoveryTest, ExtractCss_BothAttributeOrders) {
    const std::string html = R"(<head>
<link rel="stylesheet" href="/a.css">
<link href="/b.css" rel="stylesheet">
</head>)";

    const auto refs = AssetHtmlDiscovery::extractCssReferences(html);
    ASSERT_EQ(refs.size(), 2U);
    EXPECT_EQ(refs[0].href, "/a.css");
    EXPECT_EQ(refs[1].href, "/b.css");
    EXPECT_FALSE(refs[0].isExternal);
    EXPECT_FALSE(refs[1].isExternal);
}

TEST(AssetHtmlDiscoveryTest, ExtractCss_SkipsNonStylesheetLinks) {
    const std::string html = R"(<link rel="icon" href="/favicon.ico">
<link rel="stylesheet" href="/theme.css">)";

    const auto refs = AssetHtmlDiscovery::extractCssReferences(html);
    ASSERT_EQ(refs.size(), 1U);
    EXPECT_EQ(refs[0].href, "/theme.css");
}

TEST(AssetHtmlDiscoveryTest, ExtractJs_SelfClosingAndClosingTag) {
    const std::string html = R"(<script src="/a.js"></script>
<script src="/b.js"/>)";

    const auto refs = AssetHtmlDiscovery::extractJsReferences(html);
    ASSERT_EQ(refs.size(), 2U);
    EXPECT_EQ(refs[0].href, "/a.js");
    EXPECT_EQ(refs[1].href, "/b.js");
}

TEST(AssetHtmlDiscoveryTest, ExtractJs_SkipsInlineScripts) {
    const std::string html = R"(<script>console.log('inline');</script>
<script src="/app.js"></script>)";

    const auto refs = AssetHtmlDiscovery::extractJsReferences(html);
    ASSERT_EQ(refs.size(), 1U);
    EXPECT_EQ(refs[0].href, "/app.js");
}

TEST(AssetHtmlDiscoveryTest, IsExternalUrl_CdnAndProtocolRelative) {
    EXPECT_TRUE(AssetHtmlDiscovery::isExternalUrl("https://cdn.example/lib.css"));
    EXPECT_TRUE(AssetHtmlDiscovery::isExternalUrl("http://cdn.example/lib.css"));
    EXPECT_TRUE(AssetHtmlDiscovery::isExternalUrl("//cdn.example/lib.css"));
    EXPECT_FALSE(AssetHtmlDiscovery::isExternalUrl("/assets/css/local.css"));
    EXPECT_FALSE(AssetHtmlDiscovery::isExternalUrl("assets/js/app.js"));
}

TEST(AssetHtmlDiscoveryTest, ExtractRecordsTagPositions) {
    const std::string html = R"(<link rel="stylesheet" href="/x.css">)";

    const auto refs = AssetHtmlDiscovery::extractCssReferences(html);
    ASSERT_EQ(refs.size(), 1U);
    EXPECT_EQ(html.substr(refs[0].startPos, refs[0].endPos - refs[0].startPos),
              R"(<link rel="stylesheet" href="/x.css">)");
}

TEST(AssetHtmlDiscoveryTest, FilterLocalRefs_ExcludesExternalAndMissing) {
    const std::string html = R"(<link rel="stylesheet" href="https://cdn/x.css">
<link rel="stylesheet" href="/local.css">
<link rel="stylesheet" href="/missing.css">
<link rel="stylesheet" href="/excluded.css">)";

    const auto refs = AssetHtmlDiscovery::extractCssReferences(html);

    const std::unordered_set<std::string> existingPaths = {"/srv/assets/css/local.css"};
    const std::unordered_set<std::string> excludedBasenames = {"excluded.css"};

    const CssMergeDiscovery disc = AssetHtmlDiscovery::filterLocalCssRefs(
        refs,
        [](const std::string& href) -> std::string {
            if (href == "/local.css") {
                return "/srv/assets/css/local.css";
            }
            if (href == "/missing.css") {
                return "/srv/assets/css/missing.css";
            }
            if (href == "/excluded.css") {
                return "/srv/assets/css/excluded.css";
            }
            return {};
        },
        [&existingPaths](const std::string& absPath) { return existingPaths.count(absPath) > 0; },
        [&excludedBasenames](const std::string& href) {
            const size_t slash = href.find_last_of('/');
            const std::string base =
                (slash != std::string::npos) ? href.substr(slash + 1) : href;
            return excludedBasenames.count(base) > 0;
        });

    EXPECT_TRUE(disc.hasCss);
    ASSERT_EQ(disc.cssHrefs.size(), 1U);
    EXPECT_EQ(disc.cssHrefs[0], "/local.css");
    ASSERT_EQ(disc.localCssAbsolutePaths.size(), 1U);
    EXPECT_EQ(disc.localCssAbsolutePaths[0], "/srv/assets/css/local.css");
    ASSERT_EQ(disc.allCssRefs.size(), 4U);
}

TEST(AssetHtmlDiscoveryTest, MergedAssetSitePath) {
    EXPECT_EQ(AssetHtmlDiscovery::mergedAssetSitePath("page", "", ".js"), "/page.js");
    EXPECT_EQ(AssetHtmlDiscovery::mergedAssetSitePath("page", "sub", ".css"), "/sub/page.css");
    EXPECT_EQ(AssetHtmlDiscovery::mergedAssetBundleRelPath("page", "sub", ".js"), "sub/page.js");
}
