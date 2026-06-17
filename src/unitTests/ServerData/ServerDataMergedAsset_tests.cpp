#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "data/ServerData.hpp"

using namespace geruest;
namespace fs = std::filesystem;

namespace {

class ServerDataMergedAssetTest : public ::testing::Test {
   protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "geruest_merged_asset_test";
        fs::remove_all(root_);
        fs::create_directories(root_ / "html" / "secure");
        fs::create_directories(root_ / "assets" / "js" / "secure");
        fs::create_directories(root_ / "assets" / "css" / "secure");

        std::ofstream html(root_ / "html" / "secure" / "panel.html");
        html << R"(<!DOCTYPE html><html><head>
<link rel="stylesheet" href="secure/panel-base.css">
<script src="secure/panel-utils.js"></script>
<script src="secure/panel-app.js"></script>
</head><body>secret</body></html>)";

        std::ofstream css(root_ / "assets" / "css" / "secure" / "panel-base.css");
        css << "body { color: red; }\n";
        std::ofstream js1(root_ / "assets" / "js" / "secure" / "panel-utils.js");
        js1 << "const PANEL_UTILS = true;\n";
        std::ofstream js2(root_ / "assets" / "js" / "secure" / "panel-app.js");
        js2 << "function initPanel() {}\n";

        sd_.setRoot(root_.string());
        sd_.setMergeAssets(true);
        ASSERT_TRUE(sd_.addPageGate("/secure/panel", [](const HTTPRequest&) { return false; }));
    }

    void TearDown() override { fs::remove_all(root_); }

    fs::path root_;
    ServerData sd_;
};

}  // namespace

TEST_F(ServerDataMergedAssetTest, ResolvesMergedJsOwnerPagePath) {
    const auto owner = sd_.findMergedAssetOwnerPagePath("/secure/panel.js");
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, "/secure/panel");
    EXPECT_TRUE(sd_.pageRequiresAccessControl(*owner));
}

TEST_F(ServerDataMergedAssetTest, ResolvesMergedCssOwnerPagePath) {
    const auto owner = sd_.findMergedAssetOwnerPagePath("/secure/panel.css");
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, "/secure/panel");
}

TEST_F(ServerDataMergedAssetTest, UnrelatedJsPathHasNoOwner) {
    EXPECT_FALSE(sd_.findMergedAssetOwnerPagePath("/utils.js").has_value());
}

TEST_F(ServerDataMergedAssetTest, WildcardGateProtectsMergedAssetOwner) {
    ServerData sd;
    sd.setRoot(root_.string());
    sd.setMergeAssets(true);
    ASSERT_TRUE(sd.addPageGate("/secure/*", [](const HTTPRequest&) { return false; }));

    const auto owner = sd.findMergedAssetOwnerPagePath("/secure/panel.js");
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, "/secure/panel");
    EXPECT_TRUE(sd.pageRequiresAccessControl(*owner));
}
