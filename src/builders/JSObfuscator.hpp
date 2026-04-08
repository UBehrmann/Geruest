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
 *
 * Hoisting / lowering (Level 3):
 * - The renamer models var hoisting and block-scoped let/const. Do not add transforms that
 *   reorder function declarations, split chunks, or lower function declarations to var + assignment
 *   without preserving hoisting and TDZ semantics; unsafe lowering can produce "use before init"
 *   or break calls that relied on hoisting.
 * - Dynamic property access with string literals (e.g. globalThis['api']) remains the supported
 *   boundary for names that must stay stable across bundles; preserved identifiers follow the same rules.
 */

#ifndef GERUEST_JSOBFUSCATOR_HPP
#define GERUEST_JSOBFUSCATOR_HPP

#include <string>
#include <unordered_set>
#include <vector>
#include <random>

namespace geruest {

struct JSObfuscateSettings {
    /// Identifiers that keep their spelling (config + // @obfuscate:preserve in source).
    std::unordered_set<std::string> preserveIdentNames;
    /// Globals provided by the host (Closure-style); references are not flagged as undefined.
    std::unordered_set<std::string> externGlobalNames;
    /// If true, obfuscate() throws when the scope pass reports free identifiers (non-extern).
    bool strictUndefinedSymbols = false;
    /// Append globalThis['name']=name for script-level preserved bindings (stable boundary for HTML/CSS).
    bool emitGlobalThisAssignments = false;
    /// If true, run optional Acorn parse via Node when `node` and `acorn` are available.
    bool validateOutputWithAcorn = false;
};

class JSObfuscator {
public:
    explicit JSObfuscator(unsigned int level = 1);
    JSObfuscator(unsigned int level, JSObfuscateSettings settings);

    /**
     * Obfuscate JavaScript code
     * @param code The JavaScript code to obfuscate
     * @return Obfuscated JavaScript code, or original if level is 0
     * @throws std::runtime_error if strictUndefinedSymbols is true and undefined symbols are reported
     */
    std::string obfuscate(const std::string& code);

    void setLevel(unsigned int level);
    unsigned int getLevel() const { return _level; }

    void setObfuscateSettings(JSObfuscateSettings settings) { _settings = std::move(settings); }
    const JSObfuscateSettings& getObfuscateSettings() const { return _settings; }

    const std::vector<std::string>& getLastDiagnostics() const { return _lastDiagnostics; }

private:
    unsigned int _level;
    std::mt19937 _rng;
    JSObfuscateSettings _settings;
    std::vector<std::string> _lastDiagnostics;
    std::vector<std::string> _lastTopLevelPreserved;

    std::string removeWhitespace(const std::string& code);
    std::string mangleNames(const std::string& code);

    std::string encodeStrings(const std::string& code);
    std::string obfuscateNumbers(const std::string& code);

    std::string injectDeadCode(const std::string& code);
    std::string obfuscateControlFlow(const std::string& code);

    enum class TokenType {
        CODE,
        STRING_LITERAL,
        REGEX_LITERAL,
        LINE_COMMENT,
        BLOCK_COMMENT
    };

    struct Token {
        TokenType type;
        std::string text;
    };

    std::vector<Token> tokenize(const std::string& code);
    static std::string escapeForRegex(const std::string& str);
    std::string generateRandomName(int length = 8);
    std::vector<std::string> extractIdentifiers(const std::string& code);
    bool isReservedKeyword(const std::string& word);
    std::string encodeStringLiteral(const std::string& str);
    std::string createNumberExpression(int num);
    /// Optional Node + acorn parse; returns false if unavailable or parse fails.
    bool tryValidateWithAcorn(const std::string& js) const;
};

}  // namespace geruest

#endif  // GERUEST_JSOBFUSCATOR_HPP
