/**
 * @file JSObfuscator.hpp
 * @date 13.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief This class handles JavaScript code obfuscation to make code harder to analyze.
 * 
 * Obfuscation levels:
 * - Level 0: Disabled (no obfuscation)
 * - Level 1: Variable/function name mangling + whitespace removal
 * - Level 2: Level 1 + string encoding + number obfuscation
 * - Level 3: Level 2 + dead code injection + control flow obfuscation
 */

#ifndef GERUEST_JSOBFUSCATOR_HPP
#define GERUEST_JSOBFUSCATOR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>

namespace geruest {

class JSObfuscator {
public:
    /**
     * Constructor
     * @param level Obfuscation level (0=disabled, 1-3=increasing complexity)
     */
    explicit JSObfuscator(unsigned int level = 1);

    /**
     * Obfuscate JavaScript code
     * @param code The JavaScript code to obfuscate
     * @return Obfuscated JavaScript code, or original if level is 0
     */
    std::string obfuscate(const std::string& code);

    /**
     * Set obfuscation level
     * @param level 0=disabled, 1-3=increasing complexity
     */
    void setLevel(unsigned int level);

    /**
     * Get current obfuscation level
     * @return Current obfuscation level
     */
    unsigned int getLevel() const { return _level; }

private:
    unsigned int _level;
    std::mt19937 _rng;

    // Level 1: Basic obfuscation
    std::string removeWhitespace(const std::string& code);
    std::string mangleNames(const std::string& code);

    // Level 2: Intermediate obfuscation
    std::string encodeStrings(const std::string& code);
    std::string obfuscateNumbers(const std::string& code);

    // Level 3: Advanced obfuscation
    std::string injectDeadCode(const std::string& code);
    std::string obfuscateControlFlow(const std::string& code);

    // Token types for the simple JS tokenizer
    enum class TokenType {
        CODE,           // Normal code (identifiers, operators, etc.)
        STRING_LITERAL, // "...", '...', `...`
        LINE_COMMENT,   // // ...
        BLOCK_COMMENT   // /* ... */
    };

    struct Token {
        TokenType type;
        std::string text;
    };

    // Helper functions
    std::vector<Token> tokenize(const std::string& code);
    static std::string escapeForRegex(const std::string& str);
    std::string generateRandomName(int length = 8);
    std::vector<std::string> extractIdentifiers(const std::string& code);
    bool isReservedKeyword(const std::string& word);
    std::string encodeStringLiteral(const std::string& str);
    std::string createNumberExpression(int num);
    std::string processTemplateLiteral(const std::string& templateLiteral, 
                                       const std::unordered_map<std::string, std::string>& nameMap);
};

}  // namespace geruest

#endif //GERUEST_JSOBFUSCATOR_HPP
