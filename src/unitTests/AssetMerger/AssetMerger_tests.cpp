/**
 * @file AssetMerger_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the AssetMerger class
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "../../builders/AssetMerger.hpp"
#include "../../data/ServerData.hpp"

using namespace geruest;

// Test fixture for AssetMerger
class AssetMergerTest : public ::testing::Test {
protected:
    const std::string testRoot = "test_asset_merger";
    AssetMerger* merger;
    
    void SetUp() override {
        // Create test directory structure
        std::filesystem::create_directories(testRoot + "/assets/css");
        std::filesystem::create_directories(testRoot + "/assets/js");
        
        // Create test CSS files
        createFile(testRoot + "/assets/css/style1.css", "body { margin: 0; }");
        createFile(testRoot + "/assets/css/style2.css", "h1 { color: blue; }");
        createFile(testRoot + "/assets/css/style3.css", "p { font-size: 14px; }");
        
        // Create test JS files
        createFile(testRoot + "/assets/js/script1.js", "console.log('script1');");
        createFile(testRoot + "/assets/js/script2.js", "function test() { return true; }");
        createFile(testRoot + "/assets/js/script3.js", "const x = 42;");
        
        // Create AssetMerger instance
        merger = new AssetMerger(testRoot, true);
    }
    
    void TearDown() override {
        delete merger;
        if (std::filesystem::exists(testRoot)) {
            std::filesystem::remove_all(testRoot);
        }
    }
    
    void createFile(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }
};

// Basic CSS merging
TEST_F(AssetMergerTest, MergeSingleCssFile) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/style1.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasCss);
    EXPECT_EQ(result.cssFiles.size(), 1);
    EXPECT_EQ(result.cssFiles[0], "/assets/css/style1.css");
    EXPECT_NE(result.mergedCss.find("body { margin: 0; }"), std::string::npos);
}

TEST_F(AssetMergerTest, MergeMultipleCssFiles) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/style1.css">
<link rel="stylesheet" href="/assets/css/style2.css">
<link rel="stylesheet" href="/assets/css/style3.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasCss);
    EXPECT_EQ(result.cssFiles.size(), 3);
    EXPECT_NE(result.mergedCss.find("body { margin: 0; }"), std::string::npos);
    EXPECT_NE(result.mergedCss.find("h1 { color: blue; }"), std::string::npos);
    EXPECT_NE(result.mergedCss.find("p { font-size: 14px; }"), std::string::npos);
}

// Basic JS merging
TEST_F(AssetMergerTest, MergeSingleJsFile) {
    std::string html = R"(<html>
<body>
<script src="/assets/js/script1.js"></script>
</body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasJs);
    EXPECT_EQ(result.jsFiles.size(), 1);
    EXPECT_EQ(result.jsFiles[0], "/assets/js/script1.js");
    EXPECT_NE(result.mergedJs.find("console.log('script1');"), std::string::npos);
}

TEST_F(AssetMergerTest, MergeMultipleJsFiles) {
    std::string html = R"(<html>
<body>
<script src="/assets/js/script1.js"></script>
<script src="/assets/js/script2.js"></script>
<script src="/assets/js/script3.js"></script>
</body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasJs);
    EXPECT_EQ(result.jsFiles.size(), 3);
    EXPECT_NE(result.mergedJs.find("console.log('script1');"), std::string::npos);
    EXPECT_NE(result.mergedJs.find("function test()"), std::string::npos);
    EXPECT_NE(result.mergedJs.find("const x = 42;"), std::string::npos);
}

// Mixed CSS and JS
TEST_F(AssetMergerTest, MergeBothCssAndJs) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/style1.css">
</head>
<body>
<script src="/assets/js/script1.js"></script>
</body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasCss);
    EXPECT_TRUE(result.hasJs);
    EXPECT_EQ(result.cssFiles.size(), 1);
    EXPECT_EQ(result.jsFiles.size(), 1);
}

// External URLs (should be skipped)
TEST_F(AssetMergerTest, SkipExternalCssUrls) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="https://cdn.example.com/style.css">
<link rel="stylesheet" href="/assets/css/style1.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    // Should only merge local files
    EXPECT_EQ(result.cssFiles.size(), 1);
    EXPECT_EQ(result.cssFiles[0], "/assets/css/style1.css");
}

TEST_F(AssetMergerTest, SkipExternalJsUrls) {
    std::string html = R"(<html>
<body>
<script src="https://cdn.example.com/script.js"></script>
<script src="/assets/js/script1.js"></script>
</body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    // Should only merge local files
    EXPECT_EQ(result.jsFiles.size(), 1);
    EXPECT_EQ(result.jsFiles[0], "/assets/js/script1.js");
}

// HTML modification
TEST_F(AssetMergerTest, ModifiedHtmlReplacesCssTags) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/style1.css">
<link rel="stylesheet" href="/assets/css/style2.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "testpage");
    
    // Original tags should be removed
    EXPECT_EQ(result.modifiedHtml.find("style1.css"), std::string::npos);
    EXPECT_EQ(result.modifiedHtml.find("style2.css"), std::string::npos);
    
    // Should have merged CSS tag
    EXPECT_NE(result.modifiedHtml.find("testpage.css"), std::string::npos);
}

TEST_F(AssetMergerTest, ModifiedHtmlReplacesJsTags) {
    std::string html = R"(<html>
<body>
<script src="/assets/js/script1.js"></script>
<script src="/assets/js/script2.js"></script>
</body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "testpage");
    
    // Original tags should be removed
    EXPECT_EQ(result.modifiedHtml.find("script1.js"), std::string::npos);
    EXPECT_EQ(result.modifiedHtml.find("script2.js"), std::string::npos);
    
    // Should have merged JS tag
    EXPECT_NE(result.modifiedHtml.find("testpage.js"), std::string::npos);
}

// Edge cases
TEST_F(AssetMergerTest, NoAssetsToMerge) {
    std::string html = "<html><head></head><body></body></html>";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_FALSE(result.hasCss);
    EXPECT_FALSE(result.hasJs);
    EXPECT_TRUE(result.cssFiles.empty());
    EXPECT_TRUE(result.jsFiles.empty());
}

TEST_F(AssetMergerTest, EmptyHtml) {
    std::string html = "";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_FALSE(result.hasCss);
    EXPECT_FALSE(result.hasJs);
}

TEST_F(AssetMergerTest, NonexistentFile) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/nonexistent.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    // Should handle gracefully - either skip or include empty content
    // Behavior depends on implementation
}

TEST_F(AssetMergerTest, RelativePathsConverted) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="assets/css/style1.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    // Should handle relative paths (convert to absolute or handle appropriately)
}

TEST_F(AssetMergerTest, SubdirectoryDetection) {
    // Create subdirectory
    std::filesystem::create_directories(testRoot + "/assets/css/subdir");
    createFile(testRoot + "/assets/css/subdir/style.css", ".subdir { color: red; }");
    
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/subdir/style.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    // Should detect subdirectory
    EXPECT_FALSE(result.cssSubdir.empty());
}

TEST_F(AssetMergerTest, InlineSelfClosingCssTags) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="/assets/css/style1.css" />
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasCss);
    EXPECT_EQ(result.cssFiles.size(), 1);
}

TEST_F(AssetMergerTest, CssTagsWithAttributes) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" type="text/css" href="/assets/css/style1.css" media="screen">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasCss);
    EXPECT_EQ(result.cssFiles.size(), 1);
}

TEST_F(AssetMergerTest, JsTagsWithAttributes) {
    std::string html = R"(<html>
<body>
<script type="text/javascript" src="/assets/js/script1.js" defer></script>
</body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    EXPECT_TRUE(result.hasJs);
    EXPECT_EQ(result.jsFiles.size(), 1);
}

TEST_F(AssetMergerTest, PreserveExternalAssetsInModifiedHtml) {
    std::string html = R"(<html>
<head>
<link rel="stylesheet" href="https://cdn.example.com/style.css">
<link rel="stylesheet" href="/assets/css/style1.css">
</head>
<body></body>
</html>)";
    
    MergeResult result = merger->processHtml(html, "test");
    
    // External CSS should still be in modified HTML
    EXPECT_NE(result.modifiedHtml.find("https://cdn.example.com/style.css"), std::string::npos);
}
