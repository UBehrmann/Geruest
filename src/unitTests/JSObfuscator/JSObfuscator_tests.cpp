/**
 * @file JSObfuscator_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the JSObfuscator class using Google Test
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include "../../builders/JSObfuscator.hpp"

using namespace geruest;

// Test helper: Check if a string contains a substring
bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// Test helper: Check if a string matches a regex pattern
bool matchesRegex(const std::string& haystack, const std::string& pattern) {
    std::regex re(pattern);
    return std::regex_search(haystack, re);
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
    var greeting = "Hello, " + name;
    var count = 42;
    return greeting;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    EXPECT_FALSE(contains(obfuscated, "greet"));
    EXPECT_FALSE(contains(obfuscated, "greeting"));
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

TEST(JSObfuscatorTest, FilenamesNotObfuscated) {
    JSObfuscator obfuscator(2);

    // Filenames, paths, and URLs should remain readable after encoding.
    // Characters / . - _ ~ are common in filenames and must not be hex-encoded.
    std::string original = R"(
var path = "/v1/contact";
var file = "worker.js";
var asset = "/assets/css/main-style.css";
var sound = "/sound_effect.mp3";
var api = "/api/users/profile";
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // Path separators and dots must survive as-is
    EXPECT_TRUE(contains(obfuscated, "/v1/contact"))
        << "URL path must not have '/' hex-encoded";
    EXPECT_TRUE(contains(obfuscated, "worker.js"))
        << "Filename dot must not be hex-encoded";
    EXPECT_TRUE(contains(obfuscated, "/assets/css/main-style.css"))
        << "Hyphens, dots, and slashes in asset paths must not be hex-encoded";
    EXPECT_TRUE(contains(obfuscated, "/sound_effect.mp3"))
        << "Underscores and dots in filenames must not be hex-encoded";
    EXPECT_TRUE(contains(obfuscated, "/api/users/profile"))
        << "Multi-segment URL path must not be hex-encoded";
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

TEST(JSObfuscatorTest, LineCommentMinifyDoesNotSwallowNextStatement) {
    JSObfuscator obfuscator(1);
    // Strip newlines like removeWhitespace would without the // fix: the next
    // line must still parse (must not merge into // ... as comment text).
    std::string original = R"(const x=1;
// section marker
function f(){return x;}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "function"))
        << "Statement after line comment must remain code, not comment tail";
    EXPECT_FALSE(contains(obfuscated, "// section markerfunction"))
        << "Newline after // comment must be preserved before the next token";
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

    // $ prefixed identifiers should be mangled
    EXPECT_FALSE(contains(obfuscated, "$price"));
    EXPECT_FALSE(contains(obfuscated, "total$"));
    // _ prefixed identifiers are intentionally preserved (private/internal convention)
    EXPECT_TRUE(contains(obfuscated, "_count"));
    // Syntax should still be valid
    EXPECT_TRUE(contains(obfuscated, "*"));
}

TEST(JSObfuscatorTest, TemplateLiteralVariableReplacement) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function redirectToLanguagePage(language) {
    const currentPath = window.location.pathname.split('/').slice(2).join('/');
    window.location.href = `/${language}/${currentPath}`;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Function and variable names should be mangled
    EXPECT_FALSE(contains(obfuscated, "redirectToLanguagePage"));
    EXPECT_FALSE(contains(obfuscated, "language"));
    EXPECT_FALSE(contains(obfuscated, "currentPath"));
    
    // Template literal should be preserved
    EXPECT_TRUE(contains(obfuscated, "`"));
    
    // CRITICAL: Variables inside template literal expressions should also be mangled
    // They should NOT appear in their original form inside ${}
    std::regex languageInTemplate(R"(\$\{language\})");
    std::regex currentPathInTemplate(R"(\$\{currentPath\})");
    EXPECT_FALSE(std::regex_search(obfuscated, languageInTemplate))
        << "Variable 'language' inside ${} should be renamed";
    EXPECT_FALSE(std::regex_search(obfuscated, currentPathInTemplate))
        << "Variable 'currentPath' inside ${} should be renamed";
    
    // Should still have ${} expressions with some identifiers
    std::regex anyExpression(R"(\$\{[a-zA-Z_$][a-zA-Z0-9_$]*\})");
    EXPECT_TRUE(std::regex_search(obfuscated, anyExpression))
        << "Template literal should still contain ${...} expressions with mangled names";
}

TEST(JSObfuscatorTest, TemplateLiteralComplexExpressions) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function formatGreeting(user, count) {
    const greeting = `User ${user.name} has ${count} items`;
    return greeting;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Variable names should be mangled
    EXPECT_FALSE(contains(obfuscated, "formatGreeting"));
    EXPECT_FALSE(contains(obfuscated, "count"));
    EXPECT_FALSE(contains(obfuscated, "greeting"));
    
    // Member access (user.name) - "name" after dot should not be mangled
    EXPECT_TRUE(contains(obfuscated, ".name"))
        << "Property access in template literal should be preserved";
    
    // Variables inside ${} should be mangled
    std::regex countInTemplate(R"(\$\{count\})");
    EXPECT_FALSE(std::regex_search(obfuscated, countInTemplate))
        << "Variable 'count' inside ${} should be renamed";
}

TEST(JSObfuscatorTest, PreservesRegexLiteralsAndManglesOutside) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function initializePlatformClasses() {
    const ua = navigator.userAgent || '';
    if (/iPhone|iPod|IPod|iphone/i.test(ua)) {
        document.documentElement.classList.add('ios-iphone');
    }
}
)";

    std::string obfuscated = obfuscator.obfuscate(original);

    // Regression: regex literal alternatives must stay untouched.
    EXPECT_TRUE(contains(obfuscated, "/iPhone|iPod|IPod|iphone/i"))
        << "Regex literal content should not be modified by identifier mangling";

    // The local function/variable names should still be mangled.
    EXPECT_FALSE(contains(obfuscated, "initializePlatformClasses"));
    EXPECT_FALSE(contains(obfuscated, "ua="));

    // Member access and API calls should remain intact.
    EXPECT_TRUE(contains(obfuscated, ".test("));
    EXPECT_TRUE(contains(obfuscated, ".classList"));
}

TEST(JSObfuscatorTest, AsyncAwaitKeywordsPreserved) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
async function fetchData(url) {
    const response = await fetch(url);
    const data = await response.json();
    return data;
}

const handler = async (event) => {
    const result = await processEvent(event);
    return result;
};
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // async and await keywords must NOT be mangled
    EXPECT_TRUE(contains(obfuscated, "async"))
        << "The 'async' keyword must be preserved";
    EXPECT_TRUE(contains(obfuscated, "await"))
        << "The 'await' keyword must be preserved";
    
    // Function names and variables should be mangled
    EXPECT_FALSE(contains(obfuscated, "fetchData"));
    EXPECT_FALSE(contains(obfuscated, "response"));
    EXPECT_FALSE(contains(obfuscated, "data"));
    EXPECT_FALSE(contains(obfuscated, "handler"));
    EXPECT_FALSE(contains(obfuscated, "event"));
    EXPECT_FALSE(contains(obfuscated, "result"));
    
    // Check that async appears before function keyword
    size_t asyncPos = obfuscated.find("async");
    size_t functionPos = obfuscated.find("function");
    ASSERT_NE(asyncPos, std::string::npos)
        << "'async' keyword not found in obfuscated output";
    ASSERT_NE(functionPos, std::string::npos)
        << "'function' keyword not found in obfuscated output";
    EXPECT_LT(asyncPos, functionPos)
        << "async keyword should appear before function keyword";
}
TEST(JSObfuscatorTest, WebAPIsPreserved) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
async function submitForm(formData) {
    const response = await fetch("/api/submit", {
        method: "POST",
        headers: new Headers({
            "Content-Type": "application/json"
        }),
        body: JSON.stringify(formData)
    });
    
    const result = await response.json();
    
    const url = new URL(window.location.href);
    const params = new URLSearchParams(url.search);
    
    const blob = new Blob([result], { type: "application/json" });
    const formDataObj = new FormData();
    formDataObj.append("file", blob);
    
    return result;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Critical Web APIs must NOT be mangled
    EXPECT_TRUE(contains(obfuscated, "fetch"))
        << "The 'fetch' API must be preserved";
    EXPECT_TRUE(contains(obfuscated, "Headers"))
        << "The 'Headers' API must be preserved";
    EXPECT_TRUE(contains(obfuscated, "URL"))
        << "The 'URL' API must be preserved";
    EXPECT_TRUE(contains(obfuscated, "URLSearchParams"))
        << "The 'URLSearchParams' API must be preserved";
    EXPECT_TRUE(contains(obfuscated, "Blob"))
        << "The 'Blob' API must be preserved";
    EXPECT_TRUE(contains(obfuscated, "FormData"))
        << "The 'FormData' API must be preserved";
    EXPECT_TRUE(contains(obfuscated, "JSON"))
        << "The 'JSON' object must be preserved";
    
    // User-defined names should be mangled
    EXPECT_FALSE(contains(obfuscated, "submitForm"));
    EXPECT_FALSE(contains(obfuscated, "formData"));
    EXPECT_FALSE(contains(obfuscated, "response"));
    EXPECT_FALSE(contains(obfuscated, "result"));
    EXPECT_FALSE(contains(obfuscated, "params"));
    EXPECT_FALSE(contains(obfuscated, "blob"));
    EXPECT_FALSE(contains(obfuscated, "formDataObj"));
}

TEST(JSObfuscatorTest, ComprehensiveGlobalAPIsPreserved) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
// DOM & Events
const img = new Image();
const audio = new Audio("/sound.mp3");
const event = new CustomEvent("myevent");
const observer = new MutationObserver(() => {});

// Web Workers
const worker = new Worker("worker.js");
const socket = new WebSocket("ws://localhost");

// Storage
const db = indexedDB.open("mydb");

// Crypto & Intl
const hash = crypto.subtle.digest("SHA-256", data);
const formatter = new Intl.DateTimeFormat();

// Regular expressions
const regex = new RegExp("pattern");

// Error handling
throw new TypeError("Invalid type");

// Performance & Animation
performance.mark("start");
requestAnimationFrame(render);

// Modern APIs
queueMicrotask(() => console.log("microtask"));
const cloned = structuredClone(obj);
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Verify all global constructors and APIs are preserved
    EXPECT_TRUE(contains(obfuscated, "Image"));
    EXPECT_TRUE(contains(obfuscated, "Audio"));
    EXPECT_TRUE(contains(obfuscated, "CustomEvent"));
    EXPECT_TRUE(contains(obfuscated, "MutationObserver"));
    EXPECT_TRUE(contains(obfuscated, "Worker"));
    EXPECT_TRUE(contains(obfuscated, "WebSocket"));
    EXPECT_TRUE(contains(obfuscated, "indexedDB"));
    EXPECT_TRUE(contains(obfuscated, "crypto"));
    EXPECT_TRUE(contains(obfuscated, "Intl"));
    EXPECT_TRUE(contains(obfuscated, "RegExp"));
    EXPECT_TRUE(contains(obfuscated, "TypeError"));
    EXPECT_TRUE(contains(obfuscated, "performance"));
    EXPECT_TRUE(contains(obfuscated, "requestAnimationFrame"));
    EXPECT_TRUE(contains(obfuscated, "queueMicrotask"));
    EXPECT_TRUE(contains(obfuscated, "structuredClone"));
    EXPECT_TRUE(contains(obfuscated, "console"));
    
    // User variable names should be mangled
    EXPECT_FALSE(contains(obfuscated, "const img"));
    EXPECT_FALSE(contains(obfuscated, "const audio"));
    EXPECT_FALSE(contains(obfuscated, "const event"));
    EXPECT_FALSE(contains(obfuscated, "const observer"));
    EXPECT_FALSE(contains(obfuscated, "const worker"));
    EXPECT_FALSE(contains(obfuscated, "const socket"));
}

TEST(JSObfuscatorTest, NodeJSCommonJSGlobalsPreserved) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
// CommonJS module pattern
const myModule = require('./myModule');
const fs = require('fs');
const path = require('path');

exports.handler = function(event) {
    const env = process.env.NODE_ENV;
    const dir = __dirname;
    const file = __filename;
    
    const buf = Buffer.from('hello');
    global.myGlobal = 'value';
    
    setImmediate(() => {
        console.log('immediate');
    });
    
    return module.exports;
};
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Node.js/CommonJS globals must be preserved
    EXPECT_TRUE(contains(obfuscated, "require"))
        << "require() must be preserved for CommonJS";
    EXPECT_TRUE(contains(obfuscated, "exports"))
        << "exports must be preserved for CommonJS";
    EXPECT_TRUE(contains(obfuscated, "module"))
        << "module must be preserved for CommonJS";
    EXPECT_TRUE(contains(obfuscated, "process"))
        << "process must be preserved for Node.js";
    EXPECT_TRUE(contains(obfuscated, "__dirname"))
        << "__dirname must be preserved";
    EXPECT_TRUE(contains(obfuscated, "__filename"))
        << "__filename must be preserved";
    EXPECT_TRUE(contains(obfuscated, "Buffer"))
        << "Buffer must be preserved for Node.js";
    EXPECT_TRUE(contains(obfuscated, "global"))
        << "global must be preserved";
    EXPECT_TRUE(contains(obfuscated, "setImmediate"))
        << "setImmediate must be preserved";
    
    // User variables should be mangled (check for variable declarations)
    EXPECT_FALSE(contains(obfuscated, "const myModule=") || contains(obfuscated, "const myModule ="));
    EXPECT_FALSE(contains(obfuscated, "const fs=") || contains(obfuscated, "const fs ="));
    EXPECT_FALSE(contains(obfuscated, "const path=") || contains(obfuscated, "const path ="));
}

TEST(JSObfuscatorTest, ModernBrowserAPIsPreserved) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
// Fetch with AbortController
const controller = new AbortController();
const signal = controller.signal;

fetch('/api/data', { signal })
    .then(response => response.json());

setTimeout(() => controller.abort(), 5000);

// Streams API
const stream = new ReadableStream({
    start(controller) {
        controller.enqueue('data');
    }
});

// Clipboard API
const item = new ClipboardItem({ 'text/plain': blob });

// Web Components
customElements.define('my-element', MyElement);

// BigInt
const bigNum = BigInt(9007199254740991);

// Notifications
new Notification('Hello!');

// WeakRef for memory management
const ref = new WeakRef(obj);
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Modern APIs must be preserved
    EXPECT_TRUE(contains(obfuscated, "AbortController"));
    EXPECT_TRUE(contains(obfuscated, "ReadableStream"));
    EXPECT_TRUE(contains(obfuscated, "ClipboardItem"));
    EXPECT_TRUE(contains(obfuscated, "customElements"));
    EXPECT_TRUE(contains(obfuscated, "BigInt"));
    EXPECT_TRUE(contains(obfuscated, "Notification"));
    EXPECT_TRUE(contains(obfuscated, "WeakRef"));
    EXPECT_TRUE(contains(obfuscated, "fetch"));
    EXPECT_TRUE(contains(obfuscated, "setTimeout"));
    
    // User variables should be mangled (check for variable declarations, not just the name)
    EXPECT_FALSE(contains(obfuscated, "const controller=") || contains(obfuscated, "const controller ="));
    EXPECT_FALSE(contains(obfuscated, "const stream=") || contains(obfuscated, "const stream ="));
    EXPECT_FALSE(contains(obfuscated, "const item=") || contains(obfuscated, "const item ="));
    EXPECT_FALSE(contains(obfuscated, "const bigNum=") || contains(obfuscated, "const bigNum ="));
}

TEST(JSObfuscatorTest, FetchAPIPropertyNamesPreserved) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
async function submitForm(data) {
    const response = await fetch('/api/submit', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(data),
        credentials: 'include',
        mode: 'cors',
        cache: 'no-cache',
        redirect: 'follow'
    });
    
    const result = await response.json();
    
    if (response.ok) {
        console.log('Status:', response.status);
        console.log('URL:', response.url);
        return result;
    } else {
        throw new Error(result.message || 'Request failed');
    }
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Critical: Fetch API option property names MUST be preserved as object-literal keys
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bmethod\s*:)"))
        << "Fetch option 'method' must be preserved as object-literal key";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bheaders\s*:)"))
        << "Fetch option 'headers' must be preserved as object-literal key";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bbody\s*:)"))
        << "Fetch option 'body' must be preserved as object-literal key";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bcredentials\s*:)"))
        << "Fetch option 'credentials' must be preserved as object-literal key";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bmode\s*:)"))
        << "Fetch option 'mode' must be preserved as object-literal key";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bcache\s*:)"))
        << "Fetch option 'cache' must be preserved as object-literal key";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bredirect\s*:)"))
        << "Fetch option 'redirect' must be preserved as object-literal key";
    
    // Response properties must be preserved as member access
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\.ok\b)"))
        << "Response property '.ok' must be preserved as member access";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\.status\b)"))
        << "Response property '.status' must be preserved as member access";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\.url\b)"))
        << "Response property '.url' must be preserved as member access";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\.message\b)"))
        << "Common property '.message' must be preserved as member access";
    
    // User-defined names should be mangled
    EXPECT_FALSE(contains(obfuscated, "submitForm"));
    EXPECT_FALSE(contains(obfuscated, "data"));
    EXPECT_FALSE(contains(obfuscated, "response"));
    EXPECT_FALSE(contains(obfuscated, "result"));
}
TEST(JSObfuscatorTest, ShorthandAndDestructuringPreserved) {
    JSObfuscator obfuscator(1);

    // Shorthand properties: the identifier IS the property key,
    // so mangling it would silently change the wire format.
    std::string shorthand = R"(
function doFetch(endpoint) {
    const method = 'GET';
    const headers = { 'Accept': 'application/json' };
    const mode = 'cors';
    const credentials = 'same-origin';
    const cache = 'default';
    const redirect = 'follow';
    return fetch(endpoint, { method, headers, mode, credentials, cache, redirect });
}
)";
    std::string obfShort = obfuscator.obfuscate(shorthand);

    // Each reserved name must still appear as a bare word inside { ... }
    // (shorthand property syntax), not just as an object-literal key.
    EXPECT_TRUE(matchesRegex(obfShort, R"(\bmethod\b)"))
        << "Shorthand property 'method' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfShort, R"(\bheaders\b)"))
        << "Shorthand property 'headers' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfShort, R"(\bmode\b)"))
        << "Shorthand property 'mode' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfShort, R"(\bcredentials\b)"))
        << "Shorthand property 'credentials' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfShort, R"(\bcache\b)"))
        << "Shorthand property 'cache' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfShort, R"(\bredirect\b)"))
        << "Shorthand property 'redirect' must not be mangled";

    // User-defined names must still be mangled
    EXPECT_FALSE(contains(obfShort, "doFetch"));
    EXPECT_FALSE(contains(obfShort, "endpoint"));

    // Destructuring: renaming the identifier would break the mapping
    // from the property name to the local binding.
    std::string destructuring = R"(
async function handleResponse(resp) {
    const { status, ok, url, redirected } = resp;
    if (!ok) {
        throw new Error('HTTP ' + status + ' at ' + url);
    }
    const payload = await resp.json();
    const { message, code } = payload;
    return { status, message, code };
}
)";
    std::string obfDestr = obfuscator.obfuscate(destructuring);

    // Destructured property names must survive as whole words
    EXPECT_TRUE(matchesRegex(obfDestr, R"(\bstatus\b)"))
        << "Destructured property 'status' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfDestr, R"(\bok\b)"))
        << "Destructured property 'ok' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfDestr, R"(\burl\b)"))
        << "Destructured property 'url' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfDestr, R"(\bredirected\b)"))
        << "Destructured property 'redirected' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfDestr, R"(\bmessage\b)"))
        << "Destructured property 'message' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfDestr, R"(\bcode\b)"))
        << "Destructured property 'code' must not be mangled";

    // User-defined names must still be mangled
    EXPECT_FALSE(contains(obfDestr, "handleResponse"));
    EXPECT_FALSE(contains(obfDestr, "resp"));
    EXPECT_FALSE(contains(obfDestr, "payload"));
}

TEST(JSObfuscatorTest, ObjectLiteralKeysPreserved) {
    JSObfuscator obfuscator(1);
    // Object-literal keys that are NOT in RESERVED_KEYWORDS must still be
    // preserved because they define the wire/API contract (e.g. JSON keys
    // sent to a server).
    std::string original = R"(
function submitContact(formValues) {
    const payload = {
        email: formValues.email,
        phone: formValues.phone,
        address: formValues.address,
        company: formValues.company
    };
    return fetch('/api/contact', {
        method: 'POST',
        body: JSON.stringify(payload)
    });
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // Object-literal keys must not be mangled
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bemail\s*:)"))
        << "Object-literal key 'email' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bphone\s*:)"))
        << "Object-literal key 'phone' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\baddress\s*:)"))
        << "Object-literal key 'address' must not be mangled";
    EXPECT_TRUE(matchesRegex(obfuscated, R"(\bcompany\s*:)"))
        << "Object-literal key 'company' must not be mangled";

    // Member-access properties must still be preserved (dot-prefix check)
    EXPECT_TRUE(contains(obfuscated, ".email"));
    EXPECT_TRUE(contains(obfuscated, ".phone"));
    EXPECT_TRUE(contains(obfuscated, ".address"));
    EXPECT_TRUE(contains(obfuscated, ".company"));

    // User-defined variable names should still be mangled
    EXPECT_FALSE(contains(obfuscated, "submitContact"));
    EXPECT_FALSE(contains(obfuscated, "formValues"));
    EXPECT_FALSE(contains(obfuscated, "payload"));
}

TEST(JSObfuscatorTest, TernaryValuesStillMangled) {
    JSObfuscator obfuscator(1);
    // Identifiers before ':' in a ternary (preceded by '?') must still
    // be mangled — only object-literal keys are skipped.
    std::string original = R"(
function choose(flag) {
    var optionA = 10;
    var optionB = 20;
    return flag ? optionA : optionB;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    EXPECT_FALSE(contains(obfuscated, "optionA"))
        << "Ternary value 'optionA' should still be mangled";
    EXPECT_FALSE(contains(obfuscated, "optionB"))
        << "Ternary value 'optionB' should still be mangled";
    EXPECT_FALSE(contains(obfuscated, "choose"));
    EXPECT_FALSE(contains(obfuscated, "flag"));
}

TEST(JSObfuscatorTest, SwitchCaseValuesStillMangled) {
    JSObfuscator obfuscator(1);
    // Identifiers used in 'case' labels must be mangled; they are not
    // treated like object-literal keys even though they appear before ':'.
    std::string original = R"(
function selectValue(cond) {
    var myVar = 42;
    switch (cond) {
        case myVar:
            return 'hit';
        default:
            return 'miss';
    }
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // The identifier used in the case label must be mangled.
    EXPECT_FALSE(contains(obfuscated, "myVar"))
        << "Switch-case label identifier 'myVar' should still be mangled";

    // Consistency: function name and parameter should also be mangled.
    EXPECT_FALSE(contains(obfuscated, "selectValue"));
    EXPECT_FALSE(contains(obfuscated, "cond"));
}

// ============================================================
//  Number obfuscation – decimal literals and numbers in strings
// ============================================================

TEST(JSObfuscatorTest, DecimalLiteralsNotCorrupted) {
    JSObfuscator obfuscator(2);
    // Decimal fractions must remain syntactically valid after obfuscation.
    // The regex must not replace the fractional or integer part independently.
    std::string original = R"(
var opacity = 0.55;
var scale = 1.0;
var ratio = 0.28;
var fraction = 0.90;
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // After obfuscation the decimal dots must still sit between digits,
    // never followed by "0x" or "(" which would indicate the fractional
    // part was replaced with a hex literal or arithmetic expression.
    EXPECT_FALSE(matchesRegex(obfuscated, R"(\.\()" ))
        << "Fractional part must not be replaced with an expression: " << obfuscated;
    EXPECT_FALSE(matchesRegex(obfuscated, R"(\.0x)" ))
        << "Fractional part must not be replaced with a hex literal: " << obfuscated;

    // The original decimal literals should survive intact
    EXPECT_TRUE(contains(obfuscated, "0.55"))
        << "0.55 must not be corrupted: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "1.0"))
        << "1.0 must not be corrupted: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "0.28"))
        << "0.28 must not be corrupted: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "0.90"))
        << "0.90 must not be corrupted: " << obfuscated;
}

TEST(JSObfuscatorTest, NumbersInsideStringsNotObfuscated) {
    JSObfuscator obfuscator(2);
    // Numbers inside string literals must not be touched by obfuscateNumbers.
    std::string original = R"(
var percent = "100%";
var cls = "duration-500";
var label = "Step 3 of 10";
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // The string contents are hex-encoded by encodeStrings first,
    // but the digit characters should stay as literal digits (alphanumeric
    // chars are preserved by encodeStringLiteral).
    EXPECT_TRUE(contains(obfuscated, "100"))
        << "Digits inside strings must not be replaced: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "500"))
        << "Digits inside strings must not be replaced: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "10"))
        << "Digits inside strings must not be replaced: " << obfuscated;

    // Must NOT contain hex/expression replacements of those numbers inside strings
    EXPECT_FALSE(matchesRegex(obfuscated, R"(".*0x64.*")"))
        << "100 inside a string must not become 0x64: " << obfuscated;
}

TEST(JSObfuscatorTest, StandaloneIntegersStillObfuscated) {
    JSObfuscator obfuscator(2);
    // Plain integer literals in code should still be obfuscated.
    std::string original = R"(
var timeout = 5000;
var width = 768;
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    // The original integer literals should no longer appear as-is
    // (they may become hex, arithmetic, or bitshift expressions).
    EXPECT_FALSE(contains(obfuscated, "5000"))
        << "5000 should be obfuscated: " << obfuscated;
    EXPECT_FALSE(contains(obfuscated, "768"))
        << "768 should be obfuscated: " << obfuscated;
}

// Lexer regression: escaped slash inside regex literal + whitespace pass.
TEST(JSObfuscatorTest, RegexLiteralWithEscapedSlashAfterReplace) {
    JSObfuscator obfuscator(2);
    std::string original =
        "const trimmed = String(path).replace(/^\\//, '');\n";
    std::string obfuscated = obfuscator.obfuscate(original);

    EXPECT_TRUE(contains(obfuscated, "^") && contains(obfuscated, "/"))
        << "Regex literal with leading slash strip must survive: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, ".replace("))
        << "replace() call must survive: " << obfuscated;
}

// Lexer regression: double quotes inside HTML embedded in template static spans.
TEST(JSObfuscatorTest, TemplateLiteralHtmlQuotesPreserved) {
    JSObfuscator obfuscator(2);
    std::string original = R"(
disclaimer.innerHTML = `
    <span>We only use necessary cookies</span>
    <button id="accept-cookies-btn">OK</button>
`;
)";
    std::string obfuscated = obfuscator.obfuscate(original);

    EXPECT_TRUE(contains(obfuscated, "<span>"))
        << "HTML in template must not be garbled: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "accept-cookies-btn"))
        << "HTML id attribute must stay intact: " << obfuscated;
}

TEST(JSObfuscatorTest, QuerySelectorStringWithBracketsPreserved) {
    JSObfuscator obfuscator(2);
    std::string original =
        "document.querySelector('input[placeholder*=\"search\" i]');\n";
    std::string obfuscated = obfuscator.obfuscate(original);

    EXPECT_TRUE(contains(obfuscated, "placeholder") && contains(obfuscated, "search"))
        << "Selector string content must be preserved: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "querySelector"))
        << "API name should remain (reserved): " << obfuscated;
}

// Brace matching inside ${ ... } (nested object in expression).
TEST(JSObfuscatorTest, TemplateLiteralNestedBracesInSubstitution) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
const msg = `data: ${JSON.stringify({ key: 1 })}`;
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "JSON.stringify"))
        << "Nested braces in template expr should not truncate substitution: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "key"))
        << "Object key inside ${} should remain: " << obfuscated;
}

TEST(JSObfuscatorTest, LetShadowingUsesDistinctMangledNames) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
let outer = 1;
function f() {
  let outer = 2;
  return outer;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    std::regex letDecl(R"(let\s+([a-zA-Z_$][a-zA-Z0-9_$]*))");
    std::vector<std::string> names;
    for (std::sregex_iterator it(obfuscated.begin(), obfuscated.end(), letDecl), end; it != end; ++it) {
        names.push_back((*it)[1].str());
    }
    ASSERT_GE(names.size(), 2u);
    EXPECT_NE(names[0], names[1]) << obfuscated;
}

// Regression: fnPool used to be std::vector<Env>; growth reallocated storage and left
// parentFn/curEnv dangling (SIGSEGV) when obfuscating bundles with many functions.
TEST(JSObfuscatorTest, ManyTopLevelFunctionsStableScope) {
    JSObfuscator obfuscator(1);
    std::string original;
    for (int i = 0; i < 32; ++i) {
        original += "function fn" + std::to_string(i) + "(){ var local" + std::to_string(i) + " = 1; return local"
            + std::to_string(i) + ";}\n";
    }
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(obfuscated.empty());
    EXPECT_FALSE(contains(obfuscated, "local0")) << obfuscated;
}

// Regression: DOMContentLoaded (skipUntilTerminal) sees call before later function decl — must reuse
// implicit binding so mangled call matches mangled hoisted name (avoid ReferenceError).
TEST(JSObfuscatorTest, ForwardReferenceToHoistedFunctionSharesBinding) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
document.addEventListener('DOMContentLoaded', function () {
  initializeTheme();
});
function initializeTheme() { return 1; }
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(contains(obfuscated, "initializeTheme")) << obfuscated;
    std::regex declPat(R"(function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(\s*\)\s*\{)");
    std::smatch m;
    ASSERT_TRUE(std::regex_search(obfuscated, m, declPat)) << obfuscated;
    std::string mangled = m[1].str();
    EXPECT_NE(obfuscated.find(mangled + "();"), std::string::npos) << obfuscated;
}

TEST(JSObfuscatorTest, ForwardReferenceToHoistedFunctionSharesBindingLevel3) {
    JSObfuscator obfuscator(3);
    std::string original = R"(
document.addEventListener('DOMContentLoaded', function () {
  initializeFooterYear();
});
function initializeFooterYear() { return 1; }
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(contains(obfuscated, "initializeFooterYear")) << obfuscated;
    std::regex declPat(R"(function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(\s*\)\s*\{)");
    std::smatch m;
    ASSERT_TRUE(std::regex_search(obfuscated, m, declPat)) << obfuscated;
    std::string mangled = m[1].str();
    EXPECT_NE(obfuscated.find(mangled + "();"), std::string::npos) << obfuscated;
}

// Mirrors meine-buecher main.js: IIFE, template const, DOMContentLoaded without `;` after `});`,
// void (async () => { ... })(); then top-level function called from the listener.
TEST(JSObfuscatorTest, ForwardReferenceAfterVoidAsyncIifeInsideDomContentLoaded) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
(function applyInitialTheme() {
  try { localStorage.getItem('darkMode'); } catch (e) {}
})();

const apiUrl = `${window.location.origin}/v1`;

document.addEventListener("DOMContentLoaded", function () {
  initializeFooterYear();
  void (async () => {
    if (!apiUrl) return;
  })();
});
function initializeFooterYear() {
  const footerYear = document.getElementById('footer-year');
  if (footerYear) footerYear.textContent = '1';
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(contains(obfuscated, "initializeFooterYear")) << obfuscated;
    // Tight pattern: initializeFooterYear body starts with const + getElementById('footer-year')
    std::regex declPat(
        R"(function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(\s*\)\s*\{\s*const\s+[a-zA-Z_$][a-zA-Z0-9_$]*\s*=\s*document\.getElementById\(['\"]footer-year['\"]\))");
    std::smatch m;
    ASSERT_TRUE(std::regex_search(obfuscated, m, declPat)) << obfuscated;
    std::string mangled = m[1].str();
    EXPECT_EQ(countOccurrences(obfuscated, mangled), 2) << obfuscated;
    EXPECT_NE(obfuscated.find(mangled + "();"), std::string::npos) << obfuscated;
}

// Regression: for-loop + if-return + return null; then `}` must not fall through to skipUntilTerminal
// on `}` (negative brace depth swallowed clearAuthCookies and nested initializeFooterYear).
TEST(JSObfuscatorTest, GetCookieStyleForLoopThenFunctionsStayTopLevel) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function getCookie(name) {
    const nameEQ = name + '=';
    const ca = document.cookie.split(';');
    for(let i=0;i < ca.length;i++) {
        let c = ca[i];
        while (c.charAt(0)==' ') c = c.substring(1,c.length);
        if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
    }
    return null;
}

function clearAuthCookies() {
    document.cookie = "x";
}

function initializeFooterYear() {
    const footerYear = document.getElementById('footer-year');
    if (footerYear) footerYear.textContent = '1';
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(contains(obfuscated, "initializeFooterYear")) << obfuscated;
    std::regex declPat(
        R"(function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(\s*\)\s*\{\s*const\s+[a-zA-Z_$][a-zA-Z0-9_$]*\s*=\s*document\.getElementById\(['\"]footer-year['\"]\))");
    std::smatch m;
    ASSERT_TRUE(std::regex_search(obfuscated, m, declPat)) << obfuscated;
    std::string mangled = m[1].str();
    EXPECT_GE(countOccurrences(obfuscated, mangled), 1) << obfuscated;
    int fnCount = 0;
    for (size_t p = 0; (p = obfuscated.find("function ", p)) != std::string::npos; ++p) {
        ++fnCount;
    }
    EXPECT_EQ(fnCount, 3) << obfuscated;
}

// Exact slice from meine-buecher main.js (getCookie … initializeFooterYear) plus a DOMContentLoaded
// call — reproduces footer-year call/decl binding when the full file is not present.
TEST(JSObfuscatorTest, MainJsGetCookieSliceFooterYearSharesBinding) {
    JSObfuscator obfuscator(1);
    static const char* const kSlice = R"WJ1(
document.addEventListener("DOMContentLoaded",function(){
initializeFooterYear();
});
function getCookie(name) {
    const nameEQ = name + '=';
    const ca = document.cookie.split(';');
    for(let i=0;i < ca.length;i++) {
        let c = ca[i];
        while (c.charAt(0)==' ') c = c.substring(1,c.length);
        if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
    }
    return null;
}

function clearAuthCookies() {
    document.cookie = "username=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
    document.cookie = "key=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
    document.cookie = "user_id=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
}

// === Functions moved from index.js for shared functionality ===

function initializeFooterYear() {
    const footerYear = document.getElementById('footer-year');
    if (footerYear) {
        footerYear.textContent = new Date().getFullYear();
    }
}

)WJ1";
    std::string obfuscated = obfuscator.obfuscate(std::string(kSlice));
    EXPECT_FALSE(contains(obfuscated, "initializeFooterYear")) << obfuscated;
    std::regex declPat(
        R"(function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(\s*\)\s*\{\s*const\s+[a-zA-Z_$][a-zA-Z0-9_$]*\s*=\s*document\.getElementById\(['\"]footer-year['\"]\))");
    std::smatch m;
    ASSERT_TRUE(std::regex_search(obfuscated, m, declPat)) << obfuscated;
    std::string mangled = m[1].str();
    EXPECT_GE(countOccurrences(obfuscated, mangled), 2) << obfuscated;
}

TEST(JSObfuscatorTest, BracketStringKeyPreservesBareIdentifier) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
window['getCookie'] = function() { return 1; };
var x = getCookie();
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "getCookie")) << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "'getCookie'") || contains(obfuscated, "\"getCookie\"")) << obfuscated;
}

TEST(JSObfuscatorTest, BracketStringKeyDoubleQuotes) {
    JSObfuscator obfuscator(1);
    std::string original = "globalThis[\"localizedPath\"] = 1;\nfoo(localizedPath);\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "localizedPath")) << obfuscated;
}

TEST(JSObfuscatorTest, AutoBracketKeysCanBeDisabled) {
    JSObfuscateSettings st;
    st.autoPreserveBracketStringKeys = false;
    JSObfuscator obfuscator(1, st);
    std::string original = "window['api'] = 1;\nfunction g() { api(); }\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(contains(obfuscated, "api();")) << obfuscated;
}

TEST(JSObfuscatorTest, TemplateLiteralSlashAfterInterpolationIntact) {
    JSObfuscator obfuscator(1);
    std::string original = "function fn() { return 1; } var u = `x${fn()}/y`;\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_GE(countOccurrences(obfuscated, "`"), 2);
    EXPECT_TRUE(contains(obfuscated, "/y")) << obfuscated;
}

// Regression: tokenize must not treat \/ + / in /^\// as // line comment (level ≥2 uses tokenize).
// Regression: after ';' the lexer allows a regex; failed parse of "//" must not consume
// only the first slash — otherwise "// ... doesn't" is misparsed and apostrophes break strings.
TEST(JSObfuscatorTest, LineCommentSlashSlashAfterSemicolonSurvivesLevel2) {
    JSObfuscator obfuscator(2);
    std::string original =
        "function f(){return false;// Ensure form doesn't submit\n"
        "g();}\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_NE(obfuscated.find("doesn't"), std::string::npos) << obfuscated;
}

// Regression: window._languages= used to trip lodash-style passthrough and break
// HTML attribute quotes; quoted fragments must always be JSON-style encoded.
TEST(JSObfuscatorTest, Level2WindowUnderscorePropertyDoesNotBreakHtmlStrings) {
    JSObfuscator obfuscator(2);
    std::string original = R"(
window._languages = [];
function devNoticeHtml(wrap) {
  wrap.innerHTML = '<h3 id="dev-heading" class="overlay__title"></h3>';
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_EQ(obfuscated.find("innerHTML=\"<h3 id=\""), std::string::npos)
        << "double-quoted innerHTML with raw attribute quotes: " << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "dev-heading") || contains(obfuscated, "\\x22dev-heading\\x22"))
        << obfuscated;
}

// Identifiers inside nested template literals (${...} containing `...${id}...`) must rename.
TEST(JSObfuscatorTest, NestedTemplateLiteralIdentRenamed) {
    JSObfuscator obfuscator(1);
    std::string original = R"(
function pathHelper(x) { return x; }
function f() {
  const encToken = 'u1';
  return `${pathHelper(`checkUserBooks?username=${encToken}`)}`;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_FALSE(contains(obfuscated, "encToken")) << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "checkUserBooks")) << obfuscated;
}

TEST(JSObfuscatorTest, Level2PreservesRegexWithEscapedSlash) {
    JSObfuscator obfuscator(2);
    std::string original = R"(
function localizedPath(pathAndQuery) {
    const lang = getLanguageFromURL();
    const trimmed = String(pathAndQuery).replace(/^\//, '');
    const qi = trimmed.indexOf('?');
    if (qi === -1) {
        return `/${lang}/${trimmed}`;
    }
    return `/${lang}/${trimmed.slice(0, qi)}${trimmed.slice(qi)}`;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    // Regex literal is /^\/ with closing / — bytes: / ^ \ / /
    EXPECT_NE(obfuscated.find("/^\\//"), std::string::npos) << obfuscated;
    EXPECT_EQ(obfuscated.find(".replace(/^\\\n"), std::string::npos) << obfuscated;
    EXPECT_TRUE(contains(obfuscated, "${") || contains(obfuscated, "`/${")) << obfuscated;
}

// Regression: single inBacktick flag treated inner ` of nested template as closing outer —
// following `/` in markup was scanned as RegExpLiteral and corrupt output (SyntaxError: '{').
TEST(JSObfuscatorTest, NestedTemplateLiteralMinifyPreservesContent) {
    JSObfuscator obfuscator(1);
    std::string original =
        "function t(k){return k;} var authors='A'; var x = `hdr${"
        " authors ? `<div class=\"flex/basis\">${t('a')}</div>` : ''}tail`;\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "flex/basis")) << obfuscated;
    EXPECT_GE(countOccurrences(obfuscated, "`"), 4);
}

TEST(JSObfuscatorTest, QuotedPathAfterPlusSurvivesMinify) {
    JSObfuscator obfuscator(1);
    std::string original = "var a = fn() + \"/path/to/x\";\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "/path/to/x")) << obfuscated;
}

TEST(JSObfuscatorTest, PreserveDirectiveKeepsName) {
    JSObfuscator obfuscator(1);
    std::string original = "// @obfuscate:preserve apiUrl\nvar apiUrl = 1;\nvar secret = 2;\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "apiUrl")) << obfuscated;
    EXPECT_FALSE(contains(obfuscated, "secret")) << obfuscated;
}

TEST(JSObfuscatorTest, StrictUndefinedThrows) {
    JSObfuscateSettings st;
    st.strictUndefinedSymbols = true;
    JSObfuscator obfuscator(1, st);
    EXPECT_THROW(obfuscator.obfuscate("notARealGlobal();"), std::runtime_error);
}

TEST(JSObfuscatorTest, ExternSkipsStrictUndefined) {
    JSObfuscateSettings st;
    st.strictUndefinedSymbols = true;
    st.externGlobalNames.insert("notARealGlobal");
    JSObfuscator obfuscator(1, st);
    EXPECT_NO_THROW(obfuscator.obfuscate("notARealGlobal();"));
}

TEST(JSObfuscatorTest, EmitGlobalThisForPreservedTopLevelFunction) {
    JSObfuscateSettings st;
    st.emitGlobalThisAssignments = true;
    st.preserveIdentNames.insert("getCookie");
    JSObfuscator obfuscator(1, st);
    std::string original = "function getCookie() { return 1; }\n";
    std::string obfuscated = obfuscator.obfuscate(original);
    EXPECT_TRUE(contains(obfuscated, "globalThis['getCookie']")) << obfuscated;
}

// Optional: set GERUEST_RUN_ACORN_VALIDATE=1 and npm i acorn in cwd for this to pass.
TEST(JSObfuscatorTest, AcornValidationWhenNodeAndAcornAvailable) {
    if (std::getenv("GERUEST_RUN_ACORN_VALIDATE") == nullptr) {
        GTEST_SKIP() << "Set GERUEST_RUN_ACORN_VALIDATE=1 with acorn installed";
    }
    JSObfuscateSettings st;
    st.validateOutputWithAcorn = true;
    JSObfuscator obfuscator(1, st);
    std::string out = obfuscator.obfuscate("var x = 1;\n");
    EXPECT_FALSE(out.empty());
    const auto& diag = obfuscator.getLastDiagnostics();
    bool acornOk = true;
    for (const auto& line : diag) {
        if (line.find("Acorn validation failed") != std::string::npos) {
            acornOk = false;
        }
    }
    EXPECT_TRUE(acornOk) << "Install acorn: npm i acorn";
}

// Same gate: merged-style snippets that previously risked regex/division confusion after minify.
TEST(JSObfuscatorTest, AcornValidatesTemplateAndBracketPreserveGolden) {
    if (std::getenv("GERUEST_RUN_ACORN_VALIDATE") == nullptr) {
        GTEST_SKIP() << "Set GERUEST_RUN_ACORN_VALIDATE=1 with acorn installed";
    }
    JSObfuscateSettings st;
    st.validateOutputWithAcorn = true;
    JSObfuscator obfuscator(1, st);
    const char* snippets[] = {
        "function fn() { return 1; } var u = `x${fn()}/y`;\n",
        "window['getCookie'] = function(){}; getCookie();\n",
        "var a = fn() + \"/api/v1\";\nfunction fn(){return 1;}\n",
    };
    for (const char* src : snippets) {
        std::string out = obfuscator.obfuscate(src);
        EXPECT_FALSE(out.empty()) << src;
        bool acornOk = true;
        for (const auto& line : obfuscator.getLastDiagnostics()) {
            if (line.find("Acorn validation failed") != std::string::npos) {
                acornOk = false;
            }
        }
        EXPECT_TRUE(acornOk) << src << " -> " << out;
    }
}