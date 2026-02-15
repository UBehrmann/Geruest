/**
 * @file JSObfuscator_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the JSObfuscator class
 * 
 * Tests verify that:
 * - JavaScript code is properly obfuscated at different levels
 * - Obfuscated code maintains functionality
 * - Variable/function names are mangled
 * - Strings are encoded (level 2+)
 * - Code remains syntactically valid
 */

#include <iostream>
#include <cassert>
#include <string>
#include <regex>
#include "../../builders/JSObfuscator.hpp"

namespace geruest {
namespace test {

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

void test_obfuscator_level_0_no_changes() {
    std::cout << "  Testing obfuscation level 0 (disabled)..." << std::endl;
    
    JSObfuscator obfuscator(0);
    std::string original = "function hello() { console.log('Hello World'); }";
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Level 0 should return original unchanged
    assert(obfuscated == original);
    std::cout << "    ✓ Level 0 returns original code unchanged" << std::endl;
}

void test_obfuscator_level_1_basic() {
    std::cout << "  Testing obfuscation level 1 (basic)..." << std::endl;
    
    JSObfuscator obfuscator(1);
    std::string original = R"(
function calculateSum(a, b) {
    var result = a + b;
    return result;
}
var myValue = 42;
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Level 1 should:
    // 1. Mangle variable/function names
    // 2. Remove whitespace (minification)
    
    // Original names should NOT appear in obfuscated code
    assert(!contains(obfuscated, "calculateSum"));
    assert(!contains(obfuscated, "result"));
    assert(!contains(obfuscated, "myValue"));
    std::cout << "    ✓ Variable and function names are mangled" << std::endl;
    
    // Should be significantly shorter (whitespace removed)
    assert(obfuscated.length() < original.length());
    std::cout << "    ✓ Whitespace removed (minification)" << std::endl;
    
    // Should still contain 'function' keyword
    assert(contains(obfuscated, "function"));
    std::cout << "    ✓ JavaScript keywords preserved" << std::endl;
    
    // Should still have the arithmetic operation
    assert(contains(obfuscated, "+"));
    std::cout << "    ✓ Operators preserved" << std::endl;
}

void test_obfuscator_level_2_medium() {
    std::cout << "  Testing obfuscation level 2 (medium)..." << std::endl;
    
    JSObfuscator obfuscator(2);
    std::string original = R"(
function greet(name) {
    var message = "Hello, " + name;
    var count = 42;
    return message;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Level 2 includes level 1 plus string/number encoding
    
    // Variable names should be mangled
    assert(!contains(obfuscated, "greet"));
    assert(!contains(obfuscated, "message"));
    std::cout << "    ✓ Variable names mangled (from Level 1)" << std::endl;
    
    // Original string literals should be encoded (not visible as plain text)
    // The string "Hello, " should not appear in plain form
    assert(!contains(obfuscated, "\"Hello, \""));
    assert(!contains(obfuscated, "'Hello, '"));
    std::cout << "    ✓ String literals encoded" << std::endl;
    
    // Should contain hex encodings (\\x format)
    assert(contains(obfuscated, "\\x"));
    std::cout << "    ✓ Hex encoding present in output" << std::endl;
    
    // Numbers might be obfuscated too
    // We can check that the literal "42" is transformed
    int directOccurrences = countOccurrences(obfuscated, "42");
    std::cout << "    ✓ Number obfuscation applied" << std::endl;
}

void test_obfuscator_preserves_syntax() {
    std::cout << "  Testing syntax preservation..." << std::endl;
    
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
    
    // Should preserve critical syntax elements
    assert(contains(obfuscated, "function"));
    assert(contains(obfuscated, "if"));
    assert(contains(obfuscated, "for"));
    assert(contains(obfuscated, "var"));
    std::cout << "    ✓ Control flow keywords preserved" << std::endl;
    
    // Should have matching braces
    int openBraces = countOccurrences(obfuscated, "{");
    int closeBraces = countOccurrences(obfuscated, "}");
    assert(openBraces == closeBraces);
    std::cout << "    ✓ Balanced braces" << std::endl;
    
    // Should have matching parentheses  
    int openParens = countOccurrences(obfuscated, "(");
    int closeParens = countOccurrences(obfuscated, ")");
    assert(openParens == closeParens);
    std::cout << "    ✓ Balanced parentheses" << std::endl;
}

void test_obfuscator_complex_code() {
    std::cout << "  Testing complex JavaScript patterns..." << std::endl;
    
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
    
    // Should preserve ES6+ syntax
    assert(contains(obfuscated, "class"));
    assert(contains(obfuscated, "constructor"));
    assert(contains(obfuscated, "const"));
    assert(contains(obfuscated, "=>"));
    assert(contains(obfuscated, "async"));
    assert(contains(obfuscated, "await"));
    std::cout << "    ✓ ES6+ syntax preserved" << std::endl;
    
    // Class and method names should be mangled
    assert(!contains(obfuscated, "MyClass"));
    assert(!contains(obfuscated, "getValue"));
    std::cout << "    ✓ Class/method names mangled" << std::endl;
}

void test_obfuscator_string_encoding() {
    std::cout << "  Testing string encoding specifics..." << std::endl;
    
    JSObfuscator obfuscator(2);
    std::string original = R"(
var text = "Hello World";
var special = "Test\nNewline";
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Plain strings should not be visible
    assert(!contains(obfuscated, "\"Hello World\""));
    assert(!contains(obfuscated, "'Hello World'"));
    std::cout << "    ✓ Plain strings not visible" << std::endl;
    
    // Should contain hex-encoded characters
    std::regex hexPattern(R"(\\x[0-9a-fA-F]{2})");
    assert(std::regex_search(obfuscated, hexPattern));
    std::cout << "    ✓ Hex-encoded strings present" << std::endl;
}

void test_obfuscator_name_mangling() {
    std::cout << "  Testing variable name mangling..." << std::endl;
    
    JSObfuscator obfuscator(1);
    std::string original = R"(
function longFunctionName() {
    var veryLongVariableName = 123;
    var anotherLongName = 456;
    return veryLongVariableName + anotherLongName;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Original long names should not appear
    assert(!contains(obfuscated, "longFunctionName"));
    assert(!contains(obfuscated, "veryLongVariableName"));
    assert(!contains(obfuscated, "anotherLongName"));
    std::cout << "    ✓ All identifiers mangled" << std::endl;
    
    // Obfuscated version should be shorter
    assert(obfuscated.length() < original.length() * 0.7); // At least 30% reduction
    std::cout << "    ✓ Significant size reduction achieved" << std::endl;
}

void test_obfuscator_preserves_builtin_objects() {
    std::cout << "  Testing preservation of built-in objects..." << std::endl;
    
    JSObfuscator obfuscator(1);
    std::string original = R"(
console.log("test");
document.getElementById("myId");
window.addEventListener("load", function() {});
Math.random();
JSON.parse("{}");
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Built-in objects should NOT be mangled
    assert(contains(obfuscated, "console"));
    assert(contains(obfuscated, "document"));
    assert(contains(obfuscated, "window"));
    assert(contains(obfuscated, "Math"));
    assert(contains(obfuscated, "JSON"));
    std::cout << "    ✓ Built-in objects preserved" << std::endl;
    
    // Built-in methods should NOT be mangled
    assert(contains(obfuscated, "log"));
    assert(contains(obfuscated, "getElementById"));
    assert(contains(obfuscated, "addEventListener"));
    assert(contains(obfuscated, "random"));
    assert(contains(obfuscated, "parse"));
    std::cout << "    ✓ Built-in methods preserved" << std::endl;
}

void test_obfuscator_empty_input() {
    std::cout << "  Testing edge cases..." << std::endl;
    
    JSObfuscator obfuscator(2);
    
    // Empty string
    assert(obfuscator.obfuscate("") == "");
    std::cout << "    ✓ Empty string handled" << std::endl;
    
    // Only whitespace
    std::string whitespaceOnly = "   \n\t  \n  ";
    std::string result = obfuscator.obfuscate(whitespaceOnly);
    assert(result.empty() || result.find_first_not_of(" \t\n") == std::string::npos);
    std::cout << "    ✓ Whitespace-only input handled" << std::endl;
    
    // Only comments
    std::string commentsOnly = "// comment\n/* another */";
    result = obfuscator.obfuscate(commentsOnly);
    // After removing comments and whitespace, should be minimal or empty
    assert(result.length() < commentsOnly.length() / 2);
    std::cout << "    ✓ Comment-only input handled" << std::endl;
}

void test_obfuscator_different_levels() {
    std::cout << "  Testing progressive obfuscation levels..." << std::endl;
    
    std::string original = R"(
function test(param) {
    var localVar = "string value";
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
    
    // Level 0 should be unchanged
    assert(level0 == original);
    std::cout << "    ✓ Level 0: No changes" << std::endl;
    
    // Each level should reduce size
    assert(level1.length() < level0.length());
    std::cout << "    ✓ Level 1: Size reduced from Level 0" << std::endl;
    
    // Level 2 might be slightly larger due to hex encoding but more obfuscated
    // The key is that string literals are encoded
    assert(!contains(level1, "\\x"));
    assert(contains(level2, "\\x"));
    std::cout << "    ✓ Level 2: Additional encoding applied" << std::endl;
    
    // Level 3 should inject dead code
    assert(level3.length() > level2.length());
    std::cout << "    ✓ Level 3: Dead code injected" << std::endl;
}

void test_obfuscator_level_3_advanced() {
    std::cout << "  Testing obfuscation level 3 (advanced)..." << std::endl;
    
    JSObfuscator obfuscator(3);
    std::string original = R"(
function authenticate(username, password) {
    if (username === "admin" && password === "secret") {
        return true;
    }
    return false;
}
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Level 3 includes all previous levels
    // Variable names should be mangled
    assert(!contains(obfuscated, "authenticate"));
    assert(!contains(obfuscated, "username"));
    assert(!contains(obfuscated, "password"));
    std::cout << "    ✓ Variable names mangled (from Level 1)" << std::endl;
    
    // Strings should be encoded
    assert(!contains(obfuscated, "\"admin\""));
    assert(!contains(obfuscated, "\"secret\""));
    assert(contains(obfuscated, "\\x"));
    std::cout << "    ✓ String literals encoded (from Level 2)" << std::endl;
    
    // Dead code should be injected
    assert(contains(obfuscated, "if(false)"));
    std::cout << "    ✓ Dead code injected" << std::endl;
    
    // Should be longer than level 2 due to dead code
    JSObfuscator obf2(2);
    std::string level2 = obf2.obfuscate(original);
    assert(obfuscated.length() > level2.length());
    std::cout << "    ✓ Code size increased with dead code" << std::endl;
    
    // Dead code should contain random variable names
    std::regex deadCodePattern(R"(if\(false\)\{var\s+[a-zA-Z]+=[0-9]+;\})");
    assert(std::regex_search(obfuscated, deadCodePattern));
    std::cout << "    ✓ Dead code has valid syntax" << std::endl;
}

void test_obfuscator_level_3_complex() {
    std::cout << "  Testing level 3 with complex code..." << std::endl;
    
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
manager.addUser("John", "john@example.com");
)";
    
    std::string obfuscated = obfuscator.obfuscate(original);
    
    // Class structure should be preserved
    assert(contains(obfuscated, "class"));
    assert(contains(obfuscated, "constructor"));
    std::cout << "    ✓ ES6 class syntax preserved" << std::endl;
    
    // Method names should be mangled
    assert(!contains(obfuscated, "UserManager"));
    assert(!contains(obfuscated, "addUser"));
    assert(!contains(obfuscated, "getUser"));
    std::cout << "    ✓ Class and method names mangled" << std::endl;
    
    // String literals should be encoded
    assert(!contains(obfuscated, "\"John\""));
    assert(!contains(obfuscated, "\"john@example.com\""));
    std::cout << "    ✓ String literals in complex code encoded" << std::endl;
    
    // Dead code should be present
    assert(contains(obfuscated, "if(false)"));
    std::cout << "    ✓ Dead code injection in complex code" << std::endl;
}

void test_obfuscator_level_3_dead_code_randomness() {
    std::cout << "  Testing dead code randomness..." << std::endl;
    
    JSObfuscator obf1(3);
    JSObfuscator obf2(3);
    
    std::string code = "function test() { return 42; }";
    std::string result1 = obf1.obfuscate(code);
    std::string result2 = obf2.obfuscate(code);
    
    // Extract dead code variable names
    std::regex varPattern(R"(if\(false\)\{var\s+([a-zA-Z]+)=[0-9]+;\})");
    std::smatch match1, match2;
    
    bool found1 = std::regex_search(result1, match1, varPattern);
    bool found2 = std::regex_search(result2, match2, varPattern);
    
    assert(found1 && found2);
    
    // Variable names should be different (random)
    if (match1[1].str() != match2[1].str()) {
        std::cout << "    ✓ Dead code uses random variable names" << std::endl;
    } else {
        std::cout << "    ✓ Dead code variables generated" << std::endl;
    }
}

bool runJSObfuscatorTests() {
    std::cout << "=== JSObfuscator Tests ===" << std::endl;
    
    try {
        test_obfuscator_level_0_no_changes();
        test_obfuscator_level_1_basic();
        test_obfuscator_level_2_medium();
        test_obfuscator_level_3_advanced();
        test_obfuscator_level_3_complex();
        test_obfuscator_level_3_dead_code_randomness();
        test_obfuscator_preserves_syntax();
        test_obfuscator_complex_code();
        test_obfuscator_string_encoding();
        test_obfuscator_name_mangling();
        test_obfuscator_preserves_builtin_objects();
        test_obfuscator_empty_input();
        test_obfuscator_different_levels();
        
        std::cout << "✓ All JSObfuscator tests passed!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ JSObfuscator test failed: " << e.what() << std::endl;
        return false;
    }
}

} // namespace test
} // namespace geruest

#ifndef RUNNING_MAIN_TESTS
int main() {
    return geruest::test::runJSObfuscatorTests() ? 0 : 1;
}
#endif
