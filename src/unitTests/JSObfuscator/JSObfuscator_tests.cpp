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
function formatMessage(user, count) {
    const message = `User ${user.name} has ${count} items`;
    return message;
}
)";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Variable names should be mangled
    EXPECT_FALSE(contains(obfuscated, "formatMessage"));
    EXPECT_FALSE(contains(obfuscated, "count"));
    EXPECT_FALSE(contains(obfuscated, "message"));
    
    // Member access (user.name) - "name" after dot should not be mangled
    EXPECT_TRUE(contains(obfuscated, ".name"))
        << "Property access in template literal should be preserved";
    
    // Variables inside ${} should be mangled
    std::regex countInTemplate(R"(\$\{count\})");
    EXPECT_FALSE(std::regex_search(obfuscated, countInTemplate))
        << "Variable 'count' inside ${} should be renamed";
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
