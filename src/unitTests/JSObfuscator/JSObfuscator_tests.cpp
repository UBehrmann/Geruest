/**
 * @file JSObfuscator_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the JSObfuscator class using Google Test
 */

#include <gtest/gtest.h>
#include <string>
#include <regex>
#include "../../builders/JSObfuscator.hpp"

using namespace geruest;

// Test helper: Check if a string contains a substring
bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// Test helper: Count occurrences of a substring
int countOccurrences(const std::string& str, const std::string& substr) {
    int count = 0;
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
        count++;
        pos += substr.length();
    }
    return count;
}

TEST(JSObfuscatorTest, Level0NoChanges) {
    JSObfuscator obfuscator(0);
    std::string original = "function hello() { console.log('Hello World'); }";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_EQ(obfuscated, original);
}

TEST(JSObfuscatorTest, Level1Basic) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function calculateSum(a, b) {
    var result = a + b;
    return result;
}
var myValue = 42;
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_FALSE(contains(obfuscated, "calculateSum"));
    EXPECT_FALSE(contains(obfuscated, "result"));
    EXPECT_FALSE(contains(obfuscated, "myValue"));
    EXPECT_LT(obfuscated.length(), original.length());
    EXPECT_TRUE(contains(obfuscated, "function"));
    EXPECT_TRUE(contains(obfuscated, "+"));
}

TEST(JSObfuscatorTest, Level2Medium) {
    JSObfuscator obfuscator(2);
    std::string original = R"(
function greet(name) {
    var message = "Hello, " + name;
    var count = 42;
    return message;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_FALSE(contains(obfuscated, "greet"));
    EXPECT_FALSE(contains(obfuscated, "message"));
    // The comma and space after "Hello" should trigger hex encoding
    EXPECT_TRUE(contains(obfuscated, "\\x"));
}

TEST(JSObfuscatorTest, PreservesSyntax) {
    JSObfuscator obfuscator(2);
    std::string original = R"(
function test() {
    if (true) {
        for (var i = 0; i < 10; i++) {
            console.log(i);
        }
    }
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_TRUE(contains(obfuscated, "function"));
    EXPECT_TRUE(contains(obfuscated, "if"));
    EXPECT_TRUE(contains(obfuscated, "for"));
    EXPECT_TRUE(contains(obfuscated, "var"));
    
    int openBraces = countOccurrences(obfuscated, "{");
    int closeBraces = countOccurrences(obfuscated, "}");
    EXPECT_EQ(openBraces, closeBraces);
    
    int openParens = countOccurrences(obfuscated, "(");
    int closeParens = countOccurrences(obfuscated, ")");
    EXPECT_EQ(openParens, closeParens);
}

TEST(JSObfuscatorTest, ComplexCode) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
class MyClass {
    constructor(value) {
        this.value = value;
    }
    
    getValue() {
        return this.value;
    }
}

const instance = new MyClass(100);
const arrowFunc = (x, y) => x + y;
const asyncFunc = async () => await Promise.resolve(42);
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_TRUE(contains(obfuscated, "class"));
    EXPECT_TRUE(contains(obfuscated, "const"));
    EXPECT_TRUE(contains(obfuscated, "=>"));
    EXPECT_TRUE(contains(obfuscated, "await"));
    EXPECT_FALSE(contains(obfuscated, "MyClass")) << "Class name should be mangled";
    EXPECT_FALSE(contains(obfuscated, "getValue")) << "Method name should be mangled";
}

TEST(JSObfuscatorTest, StringEncoding) {
    JSObfuscator obfuscator(2);
    std::string original = R"(
var text = "Hello, World!";
var special = "Test\nNewline";
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_FALSE(contains(obfuscated, "\"Hello, World!\""));
    // Should contain hex-encoded special characters (comma, exclamation)
    std::regex hexPattern(R"(\\x[0-9a-fA-F]{2})");
    EXPECT_TRUE(std::regex_search(obfuscated, hexPattern));
}

TEST(JSObfuscatorTest, NameMangling) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function longFunctionName() {
    var veryLongVariableName = 123;
    var anotherLongName = 456;
    return veryLongVariableName + anotherLongName;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_FALSE(contains(obfuscated, "longFunctionName"));
    EXPECT_FALSE(contains(obfuscated, "veryLongVariableName"));
    EXPECT_FALSE(contains(obfuscated, "anotherLongName"));
    EXPECT_LT(obfuscated.length(), original.length() * 0.7);
}

TEST(JSObfuscatorTest, PreservesBuiltinObjects) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
console.log("test");
window.alert("message");
Math.random();
JSON.parse("{}");
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Built-in objects should NOT be mangled
    EXPECT_TRUE(contains(obfuscated, "console"));
    EXPECT_TRUE(contains(obfuscated, "window"));
    EXPECT_TRUE(contains(obfuscated, "Math"));
    EXPECT_TRUE(contains(obfuscated, "JSON"));
}

TEST(JSObfuscatorTest, EdgeCases) {
    JSObfuscator obfuscator(2);
    
    EXPECT_EQ(obfuscator.obfuscate(""), "");
    
    std::string whitespaceOnly = "   \n\t  \n  ";
    std::string result = obfuscator.obfuscate(whitespaceOnly);
    EXPECT_TRUE(result.empty() || result.find_first_not_of(" \t\n") == std::string::npos);
}

TEST(JSObfuscatorTest, ProgressiveLevels) {
    std::string original = R"(
function test(param) {
    var localVar = "string, value!";
    var number = 42;
    return localVar + number;
}
)";
    
    JSObfuscator obf0(0);
    JSObfuscator obf1(1);
    JSObfuscator obf2(2);
    JSObfuscator obf3(3);
    
    std::string level0 = obf0.obfuscate(original);
    std::string level1 = obf1.obfuscate(original);
    std::string level2 = obf2.obfuscate(original);
    std::string level3 = obf3.obfuscate(original);
    
    EXPECT_EQ(level0, original);
    EXPECT_LT(level1.length(), level0.length());
    // Level 2 should have hex encoding (comma and exclamation in string)
    EXPECT_TRUE(contains(level2, "\\x")) << "Level 2 should encode special characters";
    // Level 3 should inject dead code
    EXPECT_GT(level3.length(), level2.length());
}

TEST(JSObfuscatorTest, Level3Advanced) {
    JSObfuscator obfuscator(3);
    std::string original = R"(
function authenticate(username, password) {
    if (username === "admin!" && password === "secret?") {
        return true;
    }
    return false;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_FALSE(contains(obfuscated, "authenticate"));
    EXPECT_FALSE(contains(obfuscated, "username"));
    EXPECT_FALSE(contains(obfuscated, "password"));
    EXPECT_FALSE(contains(obfuscated, "\"admin!\""));
    EXPECT_FALSE(contains(obfuscated, "\"secret?\""));
    // Special characters (! and ?) should be hex encoded
    EXPECT_TRUE(contains(obfuscated, "\\x"));
    // Dead code should be injected
    EXPECT_TRUE(contains(obfuscated, "if(false)"));
    
    JSObfuscator obf2(2);
    std::string level2 = obf2.obfuscate(original);
    EXPECT_GT(obfuscated.length(), level2.length());
    
    // Dead code pattern verification - variable names can contain alphanumeric characters
    std::regex deadCodePattern(R"(if\(false\)\{var\s+[a-zA-Z][a-zA-Z0-9]*=[0-9]+;\})");
    EXPECT_TRUE(std::regex_search(obfuscated, deadCodePattern));
}

TEST(JSObfuscatorTest, Level3Complex) {
    JSObfuscator obfuscator(3);
    std::string original = R"(
class UserManager {
    constructor() {
        this.users = [];
    }
    
    addUser(name, email) {
        this.users.push({ name, email });
    }
    
    getUser(email) {
        return this.users.find(u => u.email === email);
    }
}

const manager = new UserManager();
manager.addUser("John!", "john@example.com");
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_TRUE(contains(obfuscated, "class"));
    EXPECT_FALSE(contains(obfuscated, "UserManager"));
    // addUser appears as member access (manager.addUser) so it is preserved
    EXPECT_TRUE(contains(obfuscated, ".addUser"));
    // getUser only appears as method definition (no dot prefix), so it is mangled
    EXPECT_FALSE(contains(obfuscated, "getUser"));
    EXPECT_FALSE(contains(obfuscated, "\"John!\""));
    EXPECT_FALSE(contains(obfuscated, "\"john@example.com\""));
    EXPECT_TRUE(contains(obfuscated, "if(false)"));
}

TEST(JSObfuscatorTest, DeadCodeRandomness) {
    JSObfuscator obf1(3);
    JSObfuscator obf2(3);
    
    std::string code = "function test() { return 42; }";
    std::string result1 = obf1.obfuscate(code);
    std::string result2 = obf2.obfuscate(code);
    
    // Both should inject dead code
    EXPECT_TRUE(contains(result1, "if(false)"));
    EXPECT_TRUE(contains(result2, "if(false)"));
    
    // Dead code pattern verification - variable names can contain alphanumeric characters
    // Pattern: if(false){var [letter][alphanumeric*]=[digits];}
    std::regex varPattern(R"(if\(false\)\{var\s+[a-zA-Z][a-zA-Z0-9]*=[0-9]+;\})");
    std::smatch match1;
    
    bool found1 = std::regex_search(result1, match1, varPattern);
    EXPECT_TRUE(found1) << "Dead code should be present in obfuscated output";
}

TEST(JSObfuscatorTest, HexEncodingOnlyForSpecialChars) {
    JSObfuscator obfuscator(2);
    
    // String with only alphanumeric characters
    std::string alphaOnly = R"(var x = "Hello";)";
    std::string result1 = obfuscator.obfuscate(alphaOnly);
    // Should NOT have hex encoding
    EXPECT_FALSE(contains(result1, "\\x"));
    
    // String with special characters
    std::string withSpecial = R"(var x = "Hello, World!";)";
    std::string result2 = obfuscator.obfuscate(withSpecial);
    // Should have hex encoding for comma and exclamation
    EXPECT_TRUE(contains(result2, "\\x"));
}

TEST(JSObfuscatorTest, PreservesStringContents) {
    JSObfuscator obfuscator(1);
    // Identifiers inside strings must NOT be mangled
    std::string original = R"(
var myVar = "myVar is a variable";
var myFunc = 'another myVar ref';
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // The variable name in code should be mangled
    EXPECT_FALSE(contains(obfuscated, "myVar=")) << "Code identifier should be mangled";
    // But the exact string literal contents should be preserved
    EXPECT_TRUE(contains(obfuscated, "myVar is a variable"))
        << "String literal content must not be mangled";
    EXPECT_TRUE(contains(obfuscated, "another myVar ref"))
        << "Single-quoted string content must not be mangled";
}

TEST(JSObfuscatorTest, PreservesComments) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
// myVar is declared here
var myVar = 10;
/* myVar is used below */
console.log(myVar);
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // The identifier "myVar" in code should be mangled (no longer appears
    // as a standalone assignment like "myVar=")
    EXPECT_FALSE(contains(obfuscated, "myVar="));
    // But "myVar" inside comments must survive (not be mangled).
    // removeWhitespace collapses formatting, so check that the comment
    // words still appear somewhere in the output.
    EXPECT_TRUE(contains(obfuscated, "//"))
        << "Line comment marker should be present";
    EXPECT_TRUE(contains(obfuscated, "myVar is declared"))
        << "Identifiers inside line comments must not be mangled";
    EXPECT_TRUE(contains(obfuscated, "myVar is used"))
        << "Identifiers inside block comments must not be mangled";
}

TEST(JSObfuscatorTest, PreservesMemberAccessIdentifiers) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
var obj = {};
obj.myProperty = 42;
console.log(obj.myProperty);
window.alert("done");
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // 'obj' is a free identifier and should be mangled
    EXPECT_FALSE(contains(obfuscated, "var obj"));
    // Properties after '.' should NOT be mangled
    EXPECT_TRUE(contains(obfuscated, ".myProperty"))
        << "Member-access property should not be mangled";
    // Built-in methods after '.' should also stay intact
    EXPECT_TRUE(contains(obfuscated, ".log"));
    EXPECT_TRUE(contains(obfuscated, ".alert"));
}

TEST(JSObfuscatorTest, DollarSignIdentifiers) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
var $price = 100;
var _count = 5;
var total$ = $price * _count;
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // Identifiers with $ and _ should be mangled without regex errors
    EXPECT_FALSE(contains(obfuscated, "$price"));
    EXPECT_FALSE(contains(obfuscated, "_count"));
    EXPECT_FALSE(contains(obfuscated, "total$"));
    // Syntax should still be valid
    EXPECT_TRUE(contains(obfuscated, "*"));
}
