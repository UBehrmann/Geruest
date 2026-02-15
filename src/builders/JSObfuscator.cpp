/**
 * @file JSObfuscator.cpp
 * @date 13.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Implementation of JavaScript obfuscator
 */

#include "JSObfuscator.hpp"
#include <algorithm>
#include <sstream>
#include <regex>
#include <chrono>
#include <iomanip>

namespace geruest {

// JavaScript reserved keywords that should not be mangled
static const std::unordered_set<std::string> RESERVED_KEYWORDS = {
    "abstract", "arguments", "await", "boolean", "break", "byte", "case", "catch",
    "char", "class", "const", "continue", "debugger", "default", "delete", "do",
    "double", "else", "enum", "eval", "export", "extends", "false", "final",
    "finally", "float", "for", "function", "goto", "if", "implements", "import",
    "in", "instanceof", "int", "interface", "let", "long", "native", "new",
    "null", "package", "private", "protected", "public", "return", "short", "static",
    "super", "switch", "synchronized", "this", "throw", "throws", "transient", "true",
    "try", "typeof", "var", "void", "volatile", "while", "with", "yield",
    // Common global objects
    "console", "window", "document", "navigator", "location", "history", "screen",
    "localStorage", "sessionStorage", "Array", "Object", "String", "Number", "Boolean",
    "Function", "Date", "Math", "JSON", "Promise", "Set", "Map", "WeakMap", "WeakSet",
    "Symbol", "Proxy", "Reflect", "parseInt", "parseFloat", "isNaN", "isFinite",
    "encodeURI", "decodeURI", "encodeURIComponent", "decodeURIComponent",
    "setTimeout", "setInterval", "clearTimeout", "clearInterval", "alert", "confirm", "prompt"
};

JSObfuscator::JSObfuscator(unsigned int level)
    : _level(level) {
    // Seed random number generator with current time
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    _rng.seed(static_cast<unsigned int>(seed));
}

std::string JSObfuscator::obfuscate(const std::string& code) {
    if (_level == 0 || code.empty()) {
        return code;
    }

    std::string result = code;

    // Level 1: Basic obfuscation
    if (_level >= 1) {
        result = mangleNames(result);
        result = removeWhitespace(result);
    }

    // Level 2: Intermediate obfuscation
    if (_level >= 2) {
        result = encodeStrings(result);
        result = obfuscateNumbers(result);
    }

    // Level 3: Advanced obfuscation
    if (_level >= 3) {
        result = injectDeadCode(result);
        result = obfuscateControlFlow(result);
    }

    return result;
}

void JSObfuscator::setLevel(unsigned int level) {
    _level = level;
}

std::string JSObfuscator::removeWhitespace(const std::string& code) {
    std::string result;
    result.reserve(code.size());
    
    bool inString = false;
    bool inSingleQuote = false;
    bool inBacktick = false;
    char prevChar = '\0';
    
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        
        // Track string literals
        if (c == '"' && prevChar != '\\' && !inSingleQuote && !inBacktick) {
            inString = !inString;
            result += c;
        } else if (c == '\'' && prevChar != '\\' && !inString && !inBacktick) {
            inSingleQuote = !inSingleQuote;
            result += c;
        } else if (c == '`' && prevChar != '\\' && !inString && !inSingleQuote) {
            inBacktick = !inBacktick;
            result += c;
        } else if (inString || inSingleQuote || inBacktick) {
            // Inside string literals, preserve everything
            result += c;
        } else if (std::isspace(c)) {
            // Outside strings, collapse whitespace
            // Keep space between alphanumeric characters
            if (!result.empty() && std::isalnum(prevChar) && i + 1 < code.size() && std::isalnum(code[i + 1])) {
                result += ' ';
            }
        } else {
            result += c;
        }
        
        prevChar = c;
    }
    
    return result;
}

std::string JSObfuscator::mangleNames(const std::string& code) {
    // Simple name mangling: replace identifiers with short random names
    // This is a simplified implementation
    std::unordered_map<std::string, std::string> nameMap;
    std::string result = code;
    
    // Extract identifiers (simplified regex for demonstration)
    std::regex identifierRegex(R"(\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b)");
    std::smatch match;
    
    auto searchStart = code.cbegin();
    std::vector<std::pair<std::string, size_t>> identifiers;
    
    while (std::regex_search(searchStart, code.cend(), match, identifierRegex)) {
        std::string identifier = match[1].str();
        if (!isReservedKeyword(identifier)) {
            identifiers.push_back({identifier, static_cast<size_t>(std::distance(code.cbegin(), match[1].first))});
        }
        searchStart = match.suffix().first;
    }
    
    // Replace identifiers with mangled names (from end to start to preserve positions)
    std::reverse(identifiers.begin(), identifiers.end());
    
    for (const auto& [identifier, pos] : identifiers) {
        if (nameMap.find(identifier) == nameMap.end()) {
            nameMap[identifier] = generateRandomName(6);
        }
        
        std::string mangledName = nameMap[identifier];
        std::regex wordBoundaryRegex("\\b" + identifier + "\\b");
        result = std::regex_replace(result, wordBoundaryRegex, mangledName);
    }
    
    return result;
}

std::string JSObfuscator::encodeStrings(const std::string& code) {
    std::string result;
    result.reserve(static_cast<size_t>(static_cast<double>(code.size()) * 1.5));  // Encoded strings are longer
    
    bool inString = false;
    bool inSingleQuote = false;
    char prevChar = '\0';
    std::string currentString;
    
    for (char c : code) {
        if (c == '"' && prevChar != '\\' && !inSingleQuote) {
            if (inString) {
                // End of string - encode it
                result += encodeStringLiteral(currentString);
                currentString.clear();
                inString = false;
            } else {
                // Start of string
                inString = true;
            }
        } else if (c == '\'' && prevChar != '\\' && !inString) {
            if (inSingleQuote) {
                // End of string - encode it
                result += encodeStringLiteral(currentString);
                currentString.clear();
                inSingleQuote = false;
            } else {
                // Start of string
                inSingleQuote = true;
            }
        } else if (inString || inSingleQuote) {
            currentString += c;
        } else {
            result += c;
        }
        
        prevChar = c;
    }
    
    return result;
}

std::string JSObfuscator::obfuscateNumbers(const std::string& code) {
    std::string result = code;
    
    // Find number literals and replace with expressions
    std::regex numberRegex(R"(\b(\d+)\b)");
    std::smatch match;
    
    // Process from end to start to preserve positions
    std::vector<std::pair<size_t, size_t>> positions;
    std::vector<int> numbers;
    
    auto searchStart = code.cbegin();
    while (std::regex_search(searchStart, code.cend(), match, numberRegex)) {
        size_t pos = std::distance(code.cbegin(), match[1].first);
        positions.push_back({pos, match[1].length()});
        numbers.push_back(std::stoi(match[1].str()));
        searchStart = match.suffix().first;
    }
    
    // Replace from end to start
    for (int i = static_cast<int>(positions.size()) - 1; i >= 0; --i) {
        std::string expression = createNumberExpression(numbers[i]);
        result.replace(positions[i].first, positions[i].second, expression);
    }
    
    return result;
}

std::string JSObfuscator::injectDeadCode(const std::string& code) {
    // Inject unreachable code blocks (simplified)
    std::string result = code;
    
    // Add some dead code at the beginning
    std::string deadCode = "if(false){var " + generateRandomName(5) + "=0;}";
    result = deadCode + result;
    
    return result;
}

std::string JSObfuscator::obfuscateControlFlow(const std::string& code) {
    // Advanced control flow obfuscation (simplified for now)
    // This would typically involve flattening control flow
    return code;
}

std::string JSObfuscator::generateRandomName(int length) {
    static const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char* charsWithDigits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    std::string name;
    name.reserve(length);
    
    // First character must be a letter
    std::uniform_int_distribution<int> letterDist(0, 51);
    name += chars[letterDist(_rng)];
    
    // Rest can be letters or digits
    std::uniform_int_distribution<int> charDist(0, 61);
    for (int i = 1; i < length; ++i) {
        name += charsWithDigits[charDist(_rng)];
    }
    
    // Ensure we didn't accidentally create a reserved word
    if (isReservedKeyword(name)) {
        return generateRandomName(length);
    }
    
    return name;
}

std::vector<std::string> JSObfuscator::extractIdentifiers(const std::string& code) {
    std::vector<std::string> identifiers;
    std::regex identifierRegex(R"(\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b)");
    
    auto searchStart = code.cbegin();
    std::smatch match;
    
    while (std::regex_search(searchStart, code.cend(), match, identifierRegex)) {
        std::string identifier = match[1].str();
        if (!isReservedKeyword(identifier)) {
            identifiers.push_back(identifier);
        }
        searchStart = match.suffix().first;
    }
    
    return identifiers;
}

bool JSObfuscator::isReservedKeyword(const std::string& word) {
    return RESERVED_KEYWORDS.find(word) != RESERVED_KEYWORDS.end();
}

std::string JSObfuscator::encodeStringLiteral(const std::string& str) {
    // Encode string to hex escape sequences
    std::ostringstream encoded;
    encoded << "\"";
    
    for (char c : str) {
        // Use hex encoding for non-alphanumeric characters
        if (std::isalnum(c) || c == ' ') {
            encoded << c;
        } else {
            encoded << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (static_cast<int>(c) & 0xFF);
        }
    }
    
    encoded << "\"";
    return encoded.str();
}

std::string JSObfuscator::createNumberExpression(int num) {
    if (num == 0) return "0";
    if (num == 1) return "1";
    
    // Create simple expressions to obfuscate numbers
    std::uniform_int_distribution<int> strategyDist(0, 2);
    int strategy = strategyDist(_rng);
    
    switch (strategy) {
        case 0:
            // Hexadecimal
            {
                std::ostringstream oss;
                oss << "0x" << std::hex << num;
                return oss.str();
            }
        case 1:
            // Simple arithmetic (if number is even)
            if (num % 2 == 0 && num > 2) {
                return "(" + std::to_string(num / 2) + "*2)";
            }
            return std::to_string(num);
        case 2:
            // Bitshift (for powers of 2)
            if (num > 1 && (num & (num - 1)) == 0) {
                int shift = 0;
                int temp = num;
                while (temp > 1) {
                    temp >>= 1;
                    shift++;
                }
                return "(1<<" + std::to_string(shift) + ")";
            }
            return std::to_string(num);
        default:
            return std::to_string(num);
    }
}

}  // namespace geruest
