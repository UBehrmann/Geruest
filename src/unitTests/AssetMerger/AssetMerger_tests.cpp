/**
 * @file AssetMerger_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the AssetMerger class
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "../../builders/AssetMerger.hpp"
#include "../../data/ServerData.hpp"

using namespace geruest;

namespace {

size_t countSubstringOccurrences(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    for (size_t pos = 0; (pos = haystack.find(needle, pos)) != std::string::npos;
         pos += needle.size(), ++count) {
    }
    return count;
}

}  // namespace

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

// Merged bundle is saved as pagebundle.js while HTML still lists that path as a source script.
// Re-merging must not treat the on-disk bundle as a fresh segment (would duplicate shared.js).
TEST_F(AssetMergerTest, NoDuplicateWhenMergedOutputOverwritesLastScriptFile) {
    createFile(testRoot + "/assets/js/shared.js", "SHARED_MARKER;");
    createFile(testRoot + "/assets/js/pagebundle.js", "PAGE_ONLY;");
    const std::string html = R"(<html><body>
<script src="shared.js"></script>
<script src="pagebundle.js"></script>
</body></html>)";

    MergeResult first = merger->processHtml(html, "pagebundle");
    ASSERT_TRUE(first.hasJs);
    EXPECT_NE(first.mergedJs.find("SHARED_MARKER;"), std::string::npos);
    EXPECT_NE(first.mergedJs.find("PAGE_ONLY;"), std::string::npos);

    {
        std::ofstream out(testRoot + "/assets/js/pagebundle.js", std::ios::binary | std::ios::trunc);
        out << first.mergedJs;
    }

    MergeResult second = merger->processHtml(html, "pagebundle");
    ASSERT_TRUE(second.hasJs);
    EXPECT_EQ(second.mergedJs, first.mergedJs);

    size_t count = 0;
    const std::string needle = "SHARED_MARKER;";
    for (size_t pos = 0; (pos = second.mergedJs.find(needle, pos)) != std::string::npos;
         pos += needle.size(), ++count) {
    }
    EXPECT_EQ(count, 1u);
}

// Plain merge (no obfuscation): each segment appears once for 2/3/4 scripts, including after
// the merged bundle is written to the last script path and the page is merged again.
TEST_F(AssetMergerTest, NoDuplicationAfterMergingTwoThreeFourJsFiles) {
    for (int n = 2; n <= 4; ++n) {
        const std::string prefix = "dupchk" + std::to_string(n);
        const std::string pageStem = prefix + "_page";
        std::vector<std::string> markers;

        std::ostringstream html;
        html << "<html><body>\n";

        for (int i = 0; i < n - 1; ++i) {
            const std::string fn = prefix + "_" + std::to_string(i) + ".js";
            const std::string marker = "__DUPSTAMP_" + prefix + "_SEG" + std::to_string(i) + "__";
            markers.push_back(marker);
            createFile(testRoot + "/assets/js/" + fn,
                       "var _s = '" + marker + "';\n");
            html << "<script src=\"" << fn << "\"></script>\n";
        }

        const std::string lastFn = pageStem + ".js";
        const std::string pageMarker = "__DUPSTAMP_" + prefix + "_PAGE__";
        markers.push_back(pageMarker);
        createFile(testRoot + "/assets/js/" + lastFn, "var _p = '" + pageMarker + "';\n");
        html << "<script src=\"" << lastFn << "\"></script>\n</body></html>";

        MergeResult first = merger->processHtml(html.str(), pageStem);
        ASSERT_TRUE(first.hasJs) << "n=" << n;
        EXPECT_EQ(first.jsFiles.size(), static_cast<size_t>(n)) << "n=" << n;

        for (const auto& marker : markers) {
            EXPECT_EQ(countSubstringOccurrences(first.mergedJs, marker), 1u)
                << "n=" << n << " after first merge, marker=" << marker;
        }

        {
            std::ofstream out(testRoot + "/assets/js/" + lastFn,
                              std::ios::binary | std::ios::trunc);
            out << first.mergedJs;
        }

        MergeResult second = merger->processHtml(html.str(), pageStem);
        ASSERT_TRUE(second.hasJs) << "n=" << n;
        EXPECT_EQ(second.mergedJs, first.mergedJs) << "n=" << n;

        for (const auto& marker : markers) {
            EXPECT_EQ(countSubstringOccurrences(second.mergedJs, marker), 1u)
                << "n=" << n << " after writeback re-merge, marker=" << marker;
        }
    }
}

TEST_F(AssetMergerTest, RemoveJsCommentsDoesNotEatRegexEscapedSlash) {
    // /^\//, '' — closing \/ + / must not be mistaken for // line comment
    createFile(testRoot + "/assets/js/regexslash.js",
               "function f(s){return String(s).replace(/^\\//, '');}\n");
    std::vector<std::string> files = {testRoot + "/assets/js/regexslash.js"};
    std::string merged = merger->mergeJsFiles(files);
    EXPECT_NE(merged.find(".replace(/^\\//"), std::string::npos) << merged;
    EXPECT_EQ(merged.find("replace(/^\\n"), std::string::npos) << merged;
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

// Regression: nested backticks inside template `${outer(`inner`)}` must not close the outer
// literal early; otherwise // inside following regex (/^\//) is mistaken for a line comment.
TEST(AssetMergerRemoveJsCommentsTest, NestedTemplatePreservesFollowingRegex) {
    const std::string js =
        "v=`${window.origin}${path(`checkUserBooks?u=${id}`)}`;\n"
        "function x(p){return String(p).replace(/^\\//,'');}\n"
        "// trailing\n";
    std::string out = AssetMerger::removeJsComments(js);
    EXPECT_NE(out.find("checkUserBooks?u=${id}"), std::string::npos) << out;
    EXPECT_NE(out.find(".replace(/^\\//"), std::string::npos) << out;
    EXPECT_EQ(out.find("trailing"), std::string::npos) << "line comment should be removed";
}

// Regression: "// foo/*.bar" contains "/*" inside the line comment; the block-comment pass must
// not treat it as opening /* ... */ paired with a later JSDoc "*/" (would erase almost the file).
TEST(AssetMergerRemoveJsCommentsTest, LineCommentWithSlashStarDoesNotEatThroughJSDoc) {
    const std::string js =
        "// Central registry — see translations/*.json\n"
        "window._x = 1;\n"
        "/**\n * doc\n */\n"
        "function localizedPath(p){return p;}\n";
    std::string out = AssetMerger::removeJsComments(js);
    EXPECT_NE(out.find("window._x = 1"), std::string::npos) << out;
    EXPECT_NE(out.find("function localizedPath"), std::string::npos) << out;
    EXPECT_EQ(out.find("translations"), std::string::npos) << out;
}
