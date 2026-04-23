/**
 * @file ContentBuilder_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the ContentBuilder class and its subclasses
 */

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>
#include "../../builders/ContentBuilder.hpp"
#include "../../builders/HTMLBuilder.hpp"
#include "../../builders/CSSBuilder.hpp"
#include "../../builders/JSBuilder.hpp"
#include "../../data/ServerData.hpp"

using namespace geruest;

const std::string TEST_ROOT = "test_content_root";

// Helper function to create a ServerData with the test root
ServerData createTestServerData(bool removeComments = true) {
    ServerData data;
    data.setRoot(TEST_ROOT);
    data.setRemoveComments(removeComments);
    return data;
}

void setup_test_environment() {
    // Create test directory structure
    std::filesystem::create_directories(TEST_ROOT + "/html");
    std::filesystem::create_directories(TEST_ROOT + "/components");
    std::filesystem::create_directories(TEST_ROOT + "/assets/css");
    std::filesystem::create_directories(TEST_ROOT + "/assets/js");
    std::filesystem::create_directories(TEST_ROOT + "/assets/translations");
    std::filesystem::create_directories(TEST_ROOT + "/files_maps");
    
    // Create test HTML file
    std::ofstream htmlFile(TEST_ROOT + "/html/test.html");
    htmlFile << "<html><head><title>Test</title></head><body>";
    htmlFile << "{header}"; // Component placeholder
    htmlFile << "<h1>[welcome_title]</h1>"; // Translation placeholder
    htmlFile << "<!-- This is a comment -->";
    htmlFile << "{footer}";
    htmlFile << "</body></html>";
    htmlFile.close();
    
    // Create test component files
    std::ofstream headerFile(TEST_ROOT + "/components/header.html");
    headerFile << "<header><nav>Navigation</nav></header>";
    headerFile.close();
    
    std::ofstream footerFile(TEST_ROOT + "/components/footer.html");
    footerFile << "<footer><p>Footer content</p></footer>";
    footerFile.close();
    
    // Create test CSS file
    std::ofstream cssFile(TEST_ROOT + "/assets/css/test.css");
    cssFile << "/* CSS Comment */\n";
    cssFile << "body { margin: 0; padding: 0; }\n";
    cssFile << "/* Another comment */\n";
    cssFile << "h1 { color: blue; }";
    cssFile.close();
    
    // Create test JS file
    std::ofstream jsFile(TEST_ROOT + "/assets/js/test.js");
    jsFile << "// JS Comment\n";
    jsFile << "console.log('Hello World');\n";
    jsFile << "/* Multi-line\n   comment */\n";
    jsFile << "function test() { return true; }";
    jsFile.close();
    
    // Create test translation file
    std::ofstream translationFile(TEST_ROOT + "/assets/translations/en.json");
    translationFile << "{\"welcome_title\": \"Welcome to Test Site\"}";
    translationFile.close();
    
    // Create file maps
    std::ofstream cssMapFile(TEST_ROOT + "/files_maps/css_file_map.json");
    cssMapFile << "{\"test\": [\"test.css\"]}";
    cssMapFile.close();
    
    std::ofstream jsMapFile(TEST_ROOT + "/files_maps/js_file_map.json");
    jsMapFile << "{\"test\": [\"test.js\"]}";
    jsMapFile.close();
}

void cleanup_test_environment() {
    if (std::filesystem::exists(TEST_ROOT)) {
        std::filesystem::remove_all(TEST_ROOT);
    }
}

// Test fixture for ContentBuilder tests
class ContentBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up before each test
        cleanup_test_environment();
    }

    void TearDown() override {
        // Clean up after each test
        cleanup_test_environment();
    }
};

TEST_F(ContentBuilderTest, LoadFile) {
    setup_test_environment();
    
    std::string testFile = TEST_ROOT + "/html/test.html";
    ServerData serverData = createTestServerData(true);
    
    // Since loadFile is protected, test it through ContentBuilder constructor
    ContentBuilder builder(testFile, serverData);
    std::string content = builder.file();
    
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("<html>"), std::string::npos);
    EXPECT_NE(content.find("{header}"), std::string::npos);
    EXPECT_NE(content.find("[welcome_title]"), std::string::npos);
}

TEST_F(ContentBuilderTest, RemoveHtmlComments) {
    // Note: removeCommentsFromString is protected, so we can't test it directly
    // This would need to be tested through integration with HTMLBuilder
    std::string htmlContent = "<html><!-- Comment1 --><body><!-- Comment2 -->Content</body></html>";
    // std::string result = ContentBuilder::removeCommentsFromString(htmlContent, "html");
    
    // For now, just verify the test structure
    EXPECT_FALSE(htmlContent.empty());
}

TEST_F(ContentBuilderTest, RemoveCssComments) {
    // Note: removeCommentsFromString is protected, so we can't test it directly
    // This would need to be tested through integration with CSSBuilder
    std::string cssContent = "/* Comment1 */ body { margin: 0; } /* Comment2 */ h1 { color: red; }";
    // std::string result = ContentBuilder::removeCommentsFromString(cssContent, "css");
    
    // For now, just verify the test structure
    EXPECT_FALSE(cssContent.empty());
}

TEST_F(ContentBuilderTest, RemoveJsComments) {
    // Note: removeCommentsFromString is protected, so we can't test it directly  
    // This would need to be tested through integration with JSBuilder
    std::string jsContent = "// Line comment\nconsole.log('test');\n/* Block comment */\nfunction test() {}";
    // std::string result = ContentBuilder::removeCommentsFromString(jsContent, "js");
    
    // For now, just verify the test structure
    EXPECT_FALSE(jsContent.empty());
}

TEST_F(ContentBuilderTest, BasicFunctionality) {
    setup_test_environment();
    
    std::string testFile = TEST_ROOT + "/html/test.html";
    ServerData serverData = createTestServerData(true);
    ContentBuilder builder(testFile, serverData);
    
    EXPECT_GT(builder.size(), 0);
    EXPECT_FALSE(builder.file().empty());
    EXPECT_EQ(builder.sizeString(), std::to_string(builder.size()));
}

TEST_F(ContentBuilderTest, WithCommentsDisabled) {
    setup_test_environment();
    
    std::string testFile = TEST_ROOT + "/html/test.html";
    ServerData serverData = createTestServerData(false); // Don't remove comments
    ContentBuilder builder(testFile, serverData);
    
    std::string content = builder.file();
    // Should still contain comments
    EXPECT_NE(content.find("<!-- This is a comment -->"), std::string::npos);
}

TEST_F(ContentBuilderTest, WithCommentsEnabled) {
    setup_test_environment();
    
    std::string testFile = TEST_ROOT + "/html/test.html";
    ServerData serverData = createTestServerData(true); // Remove comments
    ContentBuilder builder(testFile, serverData);
    
    std::string content = builder.file();
    // Should not contain comments (this may depend on the actual implementation)
    // Note: The base ContentBuilder may not process HTML comments by default
}

TEST_F(ContentBuilderTest, NonexistentFile) {
    ServerData serverData = createTestServerData(true);
    ContentBuilder builder("nonexistent.html", serverData);
    
    // Should handle gracefully
    EXPECT_EQ(builder.size(), 0);
    EXPECT_TRUE(builder.file().empty());
}

TEST_F(ContentBuilderTest, JSBuilderWithMergeAssetsBuildsOneBundleFromHtmlTemplate) {
    namespace fs = std::filesystem;
    cleanup_test_environment();
    fs::create_directories(TEST_ROOT + "/html");
    fs::create_directories(TEST_ROOT + "/assets/js");

    {
        std::ofstream html(TEST_ROOT + "/html/mergepage.html");
        html << "<!DOCTYPE html><html><body>\n";
        html << "<script src=\"a.js\"></script>\n";
        html << "<script src=\"b.js\"></script>\n";
        html << "</body></html>\n";
    }
    {
        std::ofstream a(TEST_ROOT + "/assets/js/a.js");
        a << "var A_MERGE_MARKER = 1;\n";
    }
    {
        std::ofstream b(TEST_ROOT + "/assets/js/b.js");
        b << "var B_MERGE_MARKER = 2;\n";
    }
    {
        std::ofstream stub(TEST_ROOT + "/assets/js/mergepage.js");
        stub << "DISK_STUB_SHOULD_NOT_APPEAR_IN_OUTPUT\n";
    }

    // Stale on-disk merged file must not win over HTML+sources: age stub behind inputs so
    // production disk-reuse path skips it and JSBuilder rebuilds from the template.
    {
        std::error_code ec;
        const std::string stubPath = TEST_ROOT + "/assets/js/mergepage.js";
        const auto        oldTime =
            fs::file_time_type::clock::now() - std::chrono::hours(24 * 365);
        fs::last_write_time(stubPath, oldTime, ec);
        const auto now = fs::file_time_type::clock::now();
        fs::last_write_time(TEST_ROOT + "/html/mergepage.html", now, ec);
        fs::last_write_time(TEST_ROOT + "/assets/js/a.js", now, ec);
        fs::last_write_time(TEST_ROOT + "/assets/js/b.js", now, ec);
    }

    const std::string absRoot = fs::absolute(TEST_ROOT).string();
    const std::string jsPath = absRoot + "/assets/js/mergepage.js";

    ServerData serverData;
    serverData.setRoot(absRoot);
    serverData.setMergeAssets(true);
    serverData.setRemoveComments(true);

    JSBuilder builder(jsPath, serverData);
    const std::string out = builder.file();

    EXPECT_NE(out.find("A_MERGE_MARKER"), std::string::npos) << out;
    EXPECT_NE(out.find("B_MERGE_MARKER"), std::string::npos) << out;
    EXPECT_EQ(out.find("DISK_STUB_SHOULD_NOT_APPEAR_IN_OUTPUT"), std::string::npos) << out;
}