/**
 * @file Security_tests.cpp
 * @date 27.03.2026
 *
 * @brief Unit tests for Security utilities: escapeJson, escapeHtml, escapeSql,
 *        isSafePath, and safeCombinePath.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

#include "../../security/Security.hpp"

using namespace geruest;

// ─────────────────────────────────────────────────────────────────────────────
// escapeJson
// ─────────────────────────────────────────────────────────────────────────────

class EscapeJsonTest : public ::testing::Test {};

TEST_F(EscapeJsonTest, PlainAsciiPassThrough) {
    EXPECT_EQ(Security::escapeJson("hello world"), "hello world");
}

TEST_F(EscapeJsonTest, EscapesDoubleQuote) {
    EXPECT_EQ(Security::escapeJson(R"(say "hi")"), R"(say \"hi\")");
}

TEST_F(EscapeJsonTest, EscapesBackslash) {
    EXPECT_EQ(Security::escapeJson("C:\\Users"), "C:\\\\Users");
}

TEST_F(EscapeJsonTest, EscapesNewlineAndTab) {
    EXPECT_EQ(Security::escapeJson("line1\nline2"), "line1\\nline2");
    EXPECT_EQ(Security::escapeJson("col1\tcol2"), "col1\\tcol2");
}

TEST_F(EscapeJsonTest, EscapesCarriageReturn) {
    EXPECT_EQ(Security::escapeJson("a\rb"), "a\\rb");
}

TEST_F(EscapeJsonTest, EscapesBackspaceAndFormFeed) {
    EXPECT_EQ(Security::escapeJson("a\bb"), "a\\bb");
    EXPECT_EQ(Security::escapeJson("a\fb"), "a\\fb");
}

TEST_F(EscapeJsonTest, EscapesControlCharacters) {
    // U+0001 and U+001F should become \u0001 / \u001f
    std::string input;
    input += '\x01';
    input += '\x1f';
    EXPECT_EQ(Security::escapeJson(input), "\\u0001\\u001f");
}

TEST_F(EscapeJsonTest, EmptyString) {
    EXPECT_EQ(Security::escapeJson(""), "");
}

TEST_F(EscapeJsonTest, JsonInjectionPayload) {
    // A classic JSON injection attempt: closing the string early
    std::string payload = R"(","admin":true,"x":")";
    std::string escaped = Security::escapeJson(payload);
    // The injected double-quotes must be escaped
    EXPECT_EQ(escaped.find("\\\""), 0u);
    // When wrapped in quotes the result is still valid JSON
    std::string inJson = "\"" + escaped + "\"";
    EXPECT_EQ(inJson.find("\",\"admin\""), std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// escapeHtml
// ─────────────────────────────────────────────────────────────────────────────

class EscapeHtmlTest : public ::testing::Test {};

TEST_F(EscapeHtmlTest, PlainTextPassThrough) {
    EXPECT_EQ(Security::escapeHtml("Hello World"), "Hello World");
}

TEST_F(EscapeHtmlTest, EscapesAmpersand) {
    EXPECT_EQ(Security::escapeHtml("A&B"), "A&amp;B");
}

TEST_F(EscapeHtmlTest, EscapesAngleBrackets) {
    EXPECT_EQ(Security::escapeHtml("<script>"), "&lt;script&gt;");
}

TEST_F(EscapeHtmlTest, EscapesDoubleQuote) {
    EXPECT_EQ(Security::escapeHtml(R"(He said "hi")"), "He said &quot;hi&quot;");
}

TEST_F(EscapeHtmlTest, EscapesSingleQuote) {
    EXPECT_EQ(Security::escapeHtml("It's fine"), "It&#x27;s fine");
}

TEST_F(EscapeHtmlTest, XssPayload) {
    std::string payload = "<script>alert('xss')</script>";
    std::string escaped = Security::escapeHtml(payload);
    EXPECT_EQ(escaped.find("<script>"), std::string::npos);
    EXPECT_EQ(escaped.find("&lt;script&gt;"), 0u);
}

TEST_F(EscapeHtmlTest, EmptyString) {
    EXPECT_EQ(Security::escapeHtml(""), "");
}

// ─────────────────────────────────────────────────────────────────────────────
// escapeSql
// ─────────────────────────────────────────────────────────────────────────────

class EscapeSqlTest : public ::testing::Test {};

TEST_F(EscapeSqlTest, PlainTextPassThrough) {
    EXPECT_EQ(Security::escapeSql("john"), "john");
}

TEST_F(EscapeSqlTest, EscapesSingleQuote) {
    EXPECT_EQ(Security::escapeSql("O'Brien"), "O''Brien");
}

TEST_F(EscapeSqlTest, EscapesBackslash) {
    EXPECT_EQ(Security::escapeSql("C:\\path"), "C:\\\\path");
}

TEST_F(EscapeSqlTest, EscapesNulByte) {
    std::string input;
    input += '\0';
    EXPECT_EQ(Security::escapeSql(input), "\\0");
}

TEST_F(EscapeSqlTest, EscapesDoubleQuote) {
    EXPECT_EQ(Security::escapeSql(R"(say "hi")"), R"(say \"hi\")");
}

TEST_F(EscapeSqlTest, EscapesCtrlZ) {
    std::string input;
    input += '\x1a';
    EXPECT_EQ(Security::escapeSql(input), "\\Z");
}

TEST_F(EscapeSqlTest, ClassicInjectionPayload) {
    // ' OR '1'='1  — the standard tautology injection
    std::string payload = "' OR '1'='1";
    std::string escaped = Security::escapeSql(payload);
    EXPECT_EQ(escaped, "'' OR ''1''=''1");
}

TEST_F(EscapeSqlTest, StackedQueryPayload) {
    // ; is NOT escaped because it is only syntax outside a quoted string.
    // The ' escaping traps it inside the literal, making the ; inert.
    std::string payload = "'; DROP TABLE users; --";
    std::string escaped = Security::escapeSql(payload);
    // The opening ' is doubled — the rest is literal text, never executed
    EXPECT_EQ(escaped.substr(0, 2), "''");
    // The semicolons pass through unchanged (they are harmless inside quotes)
    EXPECT_NE(escaped.find(';'), std::string::npos);
}

TEST_F(EscapeSqlTest, CommentSequencesPassThrough) {
    // -- and # and /* are also inert inside a quoted string literal
    EXPECT_NE(Security::escapeSql("value -- comment").find("--"), std::string::npos);
    EXPECT_NE(Security::escapeSql("value # comment").find('#'), std::string::npos);
    EXPECT_NE(Security::escapeSql("value /* comment */").find("/*"), std::string::npos);
}

TEST_F(EscapeSqlTest, EmptyString) {
    EXPECT_EQ(Security::escapeSql(""), "");
}

// ─────────────────────────────────────────────────────────────────────────────
// buildQuery
// ─────────────────────────────────────────────────────────────────────────────

class BuildQueryTest : public ::testing::Test {};

TEST_F(BuildQueryTest, SingleParam) {
    std::string q = Security::buildQuery("SELECT * FROM t WHERE name = ?", {"alice"});
    EXPECT_EQ(q, "SELECT * FROM t WHERE name = 'alice'");
}

TEST_F(BuildQueryTest, TwoParams) {
    std::string q = Security::buildQuery(
        "SELECT * FROM users WHERE name = ? AND email = ?",
        {"alice", "alice@example.com"});
    EXPECT_EQ(q, "SELECT * FROM users WHERE name = 'alice' AND email = 'alice@example.com'");
}

TEST_F(BuildQueryTest, ThreeParams) {
    std::string q = Security::buildQuery(
        "INSERT INTO t (a, b, c) VALUES (?, ?, ?)",
        {"x", "y", "z"});
    EXPECT_EQ(q, "INSERT INTO t (a, b, c) VALUES ('x', 'y', 'z')");
}

TEST_F(BuildQueryTest, EachParamIsEscaped) {
    std::string q = Security::buildQuery(
        "SELECT * FROM t WHERE a = ? AND b = ?",
        {"O'Brien", "say \"hi\""});
    EXPECT_EQ(q, "SELECT * FROM t WHERE a = 'O''Brien' AND b = 'say \\\"hi\\\"'");
}

TEST_F(BuildQueryTest, InjectionPayloadIsNeutralised) {
    std::string q = Security::buildQuery(
        "SELECT * FROM users WHERE name = ? AND role = ?",
        {"'; DROP TABLE users; --", "admin"});
    // The ' in the payload is doubled so it can never close the string literal.
    // The full substitution for the first parameter must be exactly this —
    // the dangerous content is trapped as string data, never parsed as SQL.
    EXPECT_EQ(q, "SELECT * FROM users WHERE name = '''; DROP TABLE users; --' AND role = 'admin'");
    // The second parameter is unaffected
    EXPECT_NE(q.find("'admin'"), std::string::npos);
}

TEST_F(BuildQueryTest, ZeroParams) {
    std::string q = Security::buildQuery("SELECT 1", {});
    EXPECT_EQ(q, "SELECT 1");
}

TEST_F(BuildQueryTest, TooFewParamsThrows) {
    EXPECT_THROW(
        Security::buildQuery("SELECT * FROM t WHERE a = ? AND b = ?", {"only_one"}),
        std::invalid_argument);
}

TEST_F(BuildQueryTest, TooManyParamsThrows) {
    EXPECT_THROW(
        Security::buildQuery("SELECT * FROM t WHERE a = ?", {"one", "two"}),
        std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// isSafePath / safeCombinePath — real on-disk roots.
// ─────────────────────────────────────────────────────────────────────────────

class PathSecurityTest : public ::testing::Test {
protected:
    std::filesystem::path sandboxDir_;
    std::string rootNative_;

    void SetUp() override {
        namespace fs = std::filesystem;
        const std::string tag =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        sandboxDir_ = fs::temp_directory_path() / ("geruest_path_sandbox_" + tag);
        fs::remove_all(sandboxDir_);
        ASSERT_TRUE(fs::create_directories(sandboxDir_ / "html"));
        ASSERT_TRUE(fs::create_directories(sandboxDir_ / "assets" / "css"));
        {
            std::ofstream f(sandboxDir_ / "html" / "index.html");
            f << 'h';
        }
        {
            std::ofstream f(sandboxDir_ / "assets" / "css" / "style.css");
            f << 'c';
        }
        rootNative_ = sandboxDir_.lexically_normal().string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(sandboxDir_, ec);
    }
};

TEST_F(PathSecurityTest, EmptyRootIsRejected) {
    EXPECT_FALSE(Security::isSafePath("", "/html/index.html"));
}

TEST_F(PathSecurityTest, NormalPathIsAlwaysSafe) {
    EXPECT_TRUE(Security::isSafePath(rootNative_, "/html/index.html"));
}

TEST_F(PathSecurityTest, PathWithoutDotDotIsAlwaysSafe) {
    EXPECT_TRUE(Security::isSafePath(rootNative_, "/assets/css/style.css"));
}

TEST_F(PathSecurityTest, DotDotTraversalIsBlocked) {
    EXPECT_FALSE(Security::isSafePath(rootNative_, "/../etc/passwd"));
}

TEST_F(PathSecurityTest, EmbeddedDotDotIsBlocked) {
    EXPECT_FALSE(Security::isSafePath(rootNative_, "/html/../../etc/passwd"));
}

TEST_F(PathSecurityTest, EncodedTraversalIsBlocked) {
    // Raw ".." without URL encoding — the URL layer has already decoded %2e%2e
    EXPECT_FALSE(Security::isSafePath(rootNative_, "/html/../../../etc/shadow"));
}

TEST_F(PathSecurityTest, SymlinkEscapeOutsideRootIsBlocked) {
    namespace fs = std::filesystem;
    const std::string tag =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path base = fs::temp_directory_path() / ("geruest_isSafePath_symlink_" + tag);
    fs::remove_all(base);
    const fs::path root = base / "site";
    const fs::path outside = base / "secret";
    ASSERT_TRUE(fs::create_directories(root / "html"));
    ASSERT_TRUE(fs::create_directories(outside));
    const fs::path link = root / "html" / "leak";
    std::error_code ec;
    fs::create_symlink(fs::path("../../secret"), link, ec);
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_FALSE(Security::isSafePath(root.string(), "/html/leak"))
        << "path under docroot must not resolve outside root via symlink";

    fs::remove_all(base, ec);
}

TEST_F(PathSecurityTest, SymlinkStayingInsideRootIsAllowed) {
    namespace fs = std::filesystem;
    const std::string tag =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path base = fs::temp_directory_path() / ("geruest_isSafePath_symlink_ok_" + tag);
    fs::remove_all(base);
    const fs::path root = base / "site";
    ASSERT_TRUE(fs::create_directories(root / "html" / "subdir"));
    const fs::path realFile = root / "html" / "subdir" / "x.txt";
    {
        std::ofstream o(realFile);
        o << 'x';
    }
    const fs::path link = root / "html" / "alias";
    std::error_code ec;
    fs::create_symlink(fs::path("subdir/x.txt"), link, ec);
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_TRUE(Security::isSafePath(root.string(), "/html/alias"));

    fs::remove_all(base, ec);
}

TEST_F(PathSecurityTest, NormalPathCombines) {
    const std::string result = Security::safeCombinePath(rootNative_, "/html/index.html");
    EXPECT_EQ(result, rootNative_ + "/html/index.html");
}

TEST_F(PathSecurityTest, TraversalReturnsEmpty) {
    EXPECT_EQ(Security::safeCombinePath(rootNative_, "/../etc/passwd"), "");
}

TEST_F(PathSecurityTest, EmptyRequestPathCombines) {
    const std::string result = Security::safeCombinePath(rootNative_, "");
    EXPECT_EQ(result, rootNative_);
}
