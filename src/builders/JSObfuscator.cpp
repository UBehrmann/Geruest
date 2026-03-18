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
#include <cctype>

namespace geruest {

// JavaScript reserved keywords that should not be mangled
static const std::unordered_set<std::string> RESERVED_KEYWORDS = {
    "abstract", "arguments", "async", "await", "boolean", "break", "byte", "case", "catch",
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
    "setTimeout", "setInterval", "clearTimeout", "clearInterval", "alert", "confirm", "prompt",
    "RegExp",
    // Modern Web APIs
    "fetch", "Request", "Response", "Headers", "XMLHttpRequest", "FormData",
    "URL", "URLSearchParams", "Blob", "File", "FileReader", "FileList",
    "TextEncoder", "TextDecoder", "atob", "btoa",
    // DOM & Events
    "Event", "CustomEvent", "MouseEvent", "KeyboardEvent", "TouchEvent", "FocusEvent",
    "Element", "HTMLElement", "Node", "NodeList", "HTMLCollection",
    "DOMParser", "MutationObserver", "IntersectionObserver", "ResizeObserver",
    // Media
    "Image", "Audio", "Video", "MediaStream", "MediaRecorder",
    // Storage & Database
    "indexedDB", "IDBDatabase", "IDBTransaction", "Cache", "CacheStorage",
    // Web Workers & Communication
    "Worker", "SharedWorker", "ServiceWorker", "WebSocket",
    "MessageChannel", "MessagePort", "BroadcastChannel",
    // Canvas & Graphics
    "ImageData", "Path2D", "WebGLRenderingContext",
    // Crypto & Security
    "crypto", "SubtleCrypto",
    // Internationalization
    "Intl",
    // Error types
    "Error", "TypeError", "ReferenceError", "SyntaxError", "RangeError", "EvalError",
    "URIError", "AggregateError",
    // Special values & globals
    "undefined", "NaN", "Infinity", "globalThis",
    "performance", "requestAnimationFrame", "cancelAnimationFrame",
    "addEventListener", "removeEventListener", "dispatchEvent",
    "queueMicrotask", "structuredClone",
    // Modern JS types
    "BigInt", "WeakRef", "FinalizationRegistry",
    // Async/Fetch Control
    "AbortController", "AbortSignal",
    // Streams API
    "ReadableStream", "WritableStream", "TransformStream",
    "ReadableStreamDefaultReader", "ReadableStreamDefaultController",
    // Web Components & Modern Browser
    "customElements", "ShadowRoot", "HTMLTemplateElement",
    "Notification", "NotificationEvent",
    "Clipboard", "ClipboardItem", "ClipboardEvent",
    // Animation & Rendering
    "Animation", "KeyframeEffect",
    // Network & Connectivity
    "NetworkInformation", "BatteryManager",
    // Deprecated but still used
    "escape", "unescape",
    // Node.js globals (for SSR/bundlers)
    "require", "exports", "module", "process", "Buffer", "global",
    "__dirname", "__filename", "setImmediate", "clearImmediate",
    // Well-known property names – reserved to protect shorthand properties
    // (e.g. { method, headers }) and destructuring (e.g. const { status } = resp).
    // Member access (response.status) is already preserved by the dot-prefix
    // check in mangleNames(), but these names must also be reserved so they
    // survive when used as bare identifiers.  Trade-off: user-defined variables
    // that happen to share a name (e.g. var status = ...) will not be mangled.
    //
    // Fetch API options
    "method", "headers", "body", "mode", "credentials", "cache", "redirect", 
    "referrer", "referrerPolicy", "integrity", "keepalive", "signal",
    // HTTP Response / Request properties
    "status", "statusText", "ok", "redirected", "type", "url",
    // Common object / Error properties
    "length", "name", "value", "message", "code", "stack", "cause",
    // Event properties
    "target", "currentTarget", "bubbles", "cancelable", "composed",
    "defaultPrevented", "eventPhase", "isTrusted", "timeStamp",
    // DOM element properties
    "id", "className", "classList", "style", "attributes", "children",
    "tagName", "innerHTML", "outerHTML", "textContent", "nodeType", "nodeName"
};

// Pre-compiled regex patterns for performance (compiled once, reused throughout)
static const std::regex IDENTIFIER_REGEX(R"(\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b)");
static const std::regex IDENTIFIER_WITH_DOT_REGEX(R"(([.]?)\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b)");

// Detect whether an identifier at [identStart, identEnd) in `segment` is an
// object-literal key.  An object-literal key is preceded by '{' or ',' (the
// start of an object literal or the separator between properties) and followed
// by ':'.  Whitespace between those characters and the identifier is skipped.
// Ternary expressions (cond ? val : …) are excluded because '?' – not '{' or ',' –
// precedes the value before ':'.
static bool isObjectLiteralKey(const std::string& segment, size_t identStart, size_t identEnd) {
    // 1) The identifier must be followed by ':' (skipping whitespace)
    bool followedByColon = false;
    for (size_t i = identEnd; i < segment.size(); ++i) {
        char c = segment[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        followedByColon = (c == ':');
        break;
    }
    if (!followedByColon) return false;

    // 2) The identifier must be preceded by '{' or ',' (skipping whitespace)
    //    This distinguishes object keys from ternary values (preceded by '?'),
    //    case labels (preceded by 'case'), and other constructs.
    for (int i = static_cast<int>(identStart) - 1; i >= 0; --i) {
        char c = segment[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        return (c == '{' || c == ',');
    }
    return false;
}

static bool isRegexAllowedAfterChar(char c) {
    switch (c) {
        case '(': case '{': case '[':
        case ',': case ';': case ':': case '?':
        case '=': case '!': case '&': case '|':
        case '+': case '-': case '*': case '%': case '^': case '~':
        case '<': case '>':
            return true;
        default:
            return false;
    }
}

static bool isRegexPrefixKeyword(const std::string& word) {
    // After these keywords, a regex literal is valid in expression position.
    static const std::unordered_set<std::string> regexKeywords = {
        "return", "throw", "case", "delete", "void", "typeof", "instanceof", "in", "of"
    };
    return regexKeywords.find(word) != regexKeywords.end();
}

static bool canStartRegexLiteral(const std::string& code, size_t slashPos) {
    // Scan backward for previous significant non-whitespace character.
    int j = static_cast<int>(slashPos) - 1;
    while (j >= 0 && std::isspace(static_cast<unsigned char>(code[static_cast<size_t>(j)]))) {
        --j;
    }

    if (j < 0) {
        return true;
    }

    const char prev = code[static_cast<size_t>(j)];
    
    // Detect postfix operators (++ and --) which are followed by division, not regex.
    // e.g., x++/2 should parse as (x++) / 2, not (x++) / (regex)
    if ((prev == '+' || prev == '-') && j > 0) {
        char beforePrev = code[static_cast<size_t>(j - 1)];
        if (beforePrev == prev) {
            // This is '++' or '--', so '/' is division, not regex start
            return false;
        }
    }
    
    if (isRegexAllowedAfterChar(prev)) {
        return true;
    }

    // Keyword-based contexts, e.g. "return /abc/".
    if (std::isalpha(static_cast<unsigned char>(prev)) || prev == '_' || prev == '$') {
        int end = j;
        int start = j;
        while (start >= 0) {
            char c = code[static_cast<size_t>(start)];
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$')) {
                break;
            }
            --start;
        }
        std::string word = code.substr(static_cast<size_t>(start + 1), static_cast<size_t>(end - start));
        if (isRegexPrefixKeyword(word)) {
            return true;
        }
    }

    return false;
}

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
    bool inRegex = false;
    bool inCharClass = false;
    char prevChar = '\0';
    
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        
        // Track string literals
        if (!inRegex && c == '"' && prevChar != '\\' && !inSingleQuote && !inBacktick) {
            inString = !inString;
            result += c;
        } else if (!inRegex && c == '\'' && prevChar != '\\' && !inString && !inBacktick) {
            inSingleQuote = !inSingleQuote;
            result += c;
        } else if (!inRegex && c == '`' && prevChar != '\\' && !inString && !inSingleQuote) {
            inBacktick = !inBacktick;
            result += c;
        } else if (inString || inSingleQuote || inBacktick) {
            // Inside string literals, preserve everything
            result += c;
        } else if (!inRegex && c == '/' && i + 1 < code.size()) {
            // Check if '/' starts a regex literal
            if (canStartRegexLiteral(code, i)) {
                inRegex = true;
                inCharClass = false;
                result += c;
            } else {
                // Regular division operator
                result += c;
            }
        } else if (inRegex) {
            // Inside regex literal, preserve everything including whitespace
            result += c;
            
            // Handle regex escape sequences
            if (c == '\\' && i + 1 < code.size()) {
                result += code[++i];  // Add escaped character
            } else if (c == '[' && prevChar != '\\') {
                inCharClass = true;
            } else if (c == ']' && prevChar != '\\' && inCharClass) {
                inCharClass = false;
            } else if (c == '/' && prevChar != '\\' && !inCharClass) {
                // End of regex literal
                inRegex = false;
                // Consume flags (g, i, m, s, u, y)
                while (i + 1 < code.size() && std::isalpha(static_cast<unsigned char>(code[i + 1]))) {
                    result += code[++i];
                }
            }
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            // Outside strings and regex, collapse whitespace
            // Keep space between alphanumeric characters
            if (!result.empty() && std::isalnum(static_cast<unsigned char>(prevChar)) && i + 1 < code.size() && std::isalnum(static_cast<unsigned char>(code[i + 1]))) {
                result += ' ';
            }
        } else {
            result += c;
        }
        
        prevChar = c;
    }
    
    return result;
}

std::vector<JSObfuscator::Token> JSObfuscator::tokenize(const std::string& code) {
    std::vector<Token> tokens;
    std::string current;
    size_t i = 0;

    auto flushCode = [&]() {
        if (!current.empty()) {
            tokens.push_back({TokenType::CODE, current});
            current.clear();
        }
    };

    while (i < code.size()) {
        char c = code[i];

        // --- line comment ---
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            flushCode();
            std::string comment;
            while (i < code.size() && code[i] != '\n') {
                comment += code[i++];
            }
            if (i < code.size()) comment += code[i++]; // include the newline
            tokens.push_back({TokenType::LINE_COMMENT, comment});
            continue;
        }

        // --- block comment ---
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            flushCode();
            std::string comment;
            comment += code[i++]; // '/'
            comment += code[i++]; // '*'
            while (i < code.size()) {
                if (code[i] == '*' && i + 1 < code.size() && code[i + 1] == '/') {
                    comment += code[i++]; // '*'
                    comment += code[i++]; // '/'
                    break;
                }
                comment += code[i++];
            }
            tokens.push_back({TokenType::BLOCK_COMMENT, comment});
            continue;
        }

        // --- string literal (double-quote, single-quote, backtick) ---
        if (c == '"' || c == '\'' || c == '`') {
            flushCode();
            char quote = c;
            std::string str;
            str += code[i++]; // opening quote
            while (i < code.size()) {
                if (code[i] == '\\') {
                    str += code[i++]; // backslash
                    if (i < code.size()) str += code[i++]; // escaped char
                    continue;
                }
                if (code[i] == quote) {
                    str += code[i++]; // closing quote
                    break;
                }
                str += code[i++];
            }
            tokens.push_back({TokenType::STRING_LITERAL, str});
            continue;
        }

        // --- regex literal (/.../flags) ---
        // We only treat '/' as regex start in expression contexts.
        if (c == '/' && canStartRegexLiteral(code, i)) {
            flushCode();
            std::string regexLiteral;
            size_t regexStart = i;  // Save position of opening '/'
            regexLiteral += code[i++]; // opening '/'

            bool inCharClass = false;
            bool foundClosing = false;
            while (i < code.size()) {
                char rc = code[i];

                if (rc == '\\') {
                    regexLiteral += code[i++];
                    if (i < code.size()) regexLiteral += code[i++];
                    continue;
                }

                if (rc == '[') {
                    inCharClass = true;
                    regexLiteral += code[i++];
                    continue;
                }

                if (rc == ']' && inCharClass) {
                    inCharClass = false;
                    regexLiteral += code[i++];
                    continue;
                }

                if (rc == '/' && !inCharClass) {
                    regexLiteral += code[i++]; // closing '/'
                    while (i < code.size() && std::isalpha(static_cast<unsigned char>(code[i]))) {
                        regexLiteral += code[i++];
                    }
                    foundClosing = true;
                    break;
                }

                regexLiteral += code[i++];
            }

            // Only emit REGEX_LITERAL if we found a valid closing '/'.
            // Otherwise, treat '/' as normal code (division operator).
            if (foundClosing) {
                tokens.push_back({TokenType::REGEX_LITERAL, regexLiteral});
            } else {
                // Restore position to just after the opening '/',
                // treat '/' as division operator, and continue normal processing
                i = regexStart + 1;
                current += '/';
            }
            continue;
        }

        current += code[i++];
    }
    flushCode();
    return tokens;
}

std::string JSObfuscator::escapeForRegex(const std::string& str) {
    static const std::string metacharacters = R"(\^$.|?*+()[]{}-)";
    std::string escaped;
    escaped.reserve(str.size() * 2);
    for (char c : str) {
        if (metacharacters.find(c) != std::string::npos) {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

std::string JSObfuscator::processTemplateLiteral(const std::string& templateLiteral,
                                                 const std::unordered_map<std::string, std::string>& nameMap) {
    // Template literals are in the form: `text ${expr} more text ${expr2}`
    // We need to find ${...} expressions and replace variable names inside them
    
    // Pre-allocate with extra space for potentially longer mangled names
    // Estimate: original size + 30% for mangled names which may be longer/shorter
    std::string result;
    const size_t estimatedCapacity = templateLiteral.size() + (templateLiteral.size() / 3) + 1;
    result.reserve(estimatedCapacity);
    
    size_t pos = 0;
    while (pos < templateLiteral.size()) {
        // Look for ${
        size_t exprStart = templateLiteral.find("${", pos);
        
        if (exprStart == std::string::npos) {
            // No more expressions, copy rest of the string
            result.append(templateLiteral, pos, std::string::npos);
            break;
        }
        
        // Copy everything before the expression (including "${")
        result.append(templateLiteral, pos, exprStart - pos + 2);
        
        // Find the matching }
        size_t exprEnd = templateLiteral.find("}", exprStart + 2);
        if (exprEnd == std::string::npos) {
            // Malformed template literal, copy rest as-is
            result.append(templateLiteral, exprStart + 2, std::string::npos);
            break;
        }
        
        // Extract the expression content
        size_t exprContentStart = exprStart + 2;
        size_t exprContentLen = exprEnd - exprContentStart;
        
        // Use string_view-like approach to avoid copying when possible
        const char* exprStart_ptr = templateLiteral.data() + exprContentStart;
        std::string expression(exprStart_ptr, exprContentLen);
        
        // Replace identifiers in the expression using the name map
        // Use the pre-compiled static regex for performance
        std::sregex_iterator it(expression.begin(), expression.end(), IDENTIFIER_REGEX);
        std::sregex_iterator endIt;
        size_t lastExprPos = 0;
        
        while (it != endIt) {
            const std::smatch& m = *it;
            size_t matchStart = static_cast<size_t>(m.position());
            size_t matchLen = static_cast<size_t>(m.length());
            std::string identifier = m[1].str();
            
            // Append text before the match
            result.append(expression, lastExprPos, matchStart - lastExprPos);
            
            // Check if preceded by '.' (member access)
            bool isMemberAccess = (matchStart > 0 && expression[matchStart - 1] == '.');
            
            if (!isMemberAccess) {
                auto mapIt = nameMap.find(identifier);
                if (mapIt != nameMap.end()) {
                    result.append(mapIt->second);
                } else {
                    result.append(identifier);
                }
            } else {
                result.append(identifier);
            }
            
            lastExprPos = matchStart + matchLen;
            ++it;
        }
        
        // Append remaining text after last match in expression and closing }
        result.append(expression, lastExprPos, std::string::npos);
        result.push_back('}');
        
        pos = exprEnd + 1;
    }
    
    return result;
}

std::string JSObfuscator::mangleNames(const std::string& code) {
    // Tokenize the source so we can skip strings and comments
    std::vector<Token> tokens = tokenize(code);

    // 1) Collect identifiers that appear in CODE tokens,
    //    skipping those preceded by '.' (member access).
    std::unordered_map<std::string, std::string> nameMap;

    for (const auto& tok : tokens) {
        if (tok.type != TokenType::CODE) continue;

        auto searchStart = tok.text.cbegin();
        std::smatch match;
        while (std::regex_search(searchStart, tok.text.cend(), match, IDENTIFIER_WITH_DOT_REGEX)) {
            std::string dotPrefix = match[1].str();
            std::string identifier = match[2].str();

            // Compute absolute position of group 2 within tok.text
            size_t offset = static_cast<size_t>(searchStart - tok.text.cbegin());
            size_t identStart = offset + static_cast<size_t>(match.position(2));
            size_t identEnd = identStart + identifier.length();

            // Skip member-access identifiers (preceded by '.'),
            // reserved words, identifiers starting with '_' (internal/private),
            // and object-literal keys (e.g. { email: … })
            if (dotPrefix.empty() && !isReservedKeyword(identifier) 
                && !identifier.empty() && identifier[0] != '_'
                && !isObjectLiteralKey(tok.text, identStart, identEnd)) {
                if (nameMap.find(identifier) == nameMap.end()) {
                    nameMap[identifier] = generateRandomName(6);
                }
            }
            searchStart = match.suffix().first;
        }
    }

    // 2) Replace identifiers only inside CODE tokens, using position-based
    //    replacement to avoid lookbehind (unsupported by std::regex).
    std::string result;
    result.reserve(code.size());

    for (const auto& tok : tokens) {
        if (tok.type != TokenType::CODE) {
            // Check if it's a template literal (starts and ends with backtick)
            if (tok.type == TokenType::STRING_LITERAL && 
                tok.text.size() >= 2 && 
                tok.text[0] == '`' && 
                tok.text[tok.text.size() - 1] == '`') {
                // Process template literal to replace variables inside ${...}
                result += processTemplateLiteral(tok.text, nameMap);
            } else {
                // Preserve strings, comments verbatim
                result += tok.text;
            }
            continue;
        }

        // Walk through the code segment and replace identifiers
        // that are NOT preceded by '.'
        const std::string& segment = tok.text;
        std::string replaced;
        replaced.reserve(segment.size());

        std::sregex_iterator it(segment.begin(), segment.end(), IDENTIFIER_REGEX);
        std::sregex_iterator endIt;
        size_t lastPos = 0;

        while (it != endIt) {
            const std::smatch& m = *it;
            size_t matchStart = static_cast<size_t>(m.position());
            size_t matchLen = static_cast<size_t>(m.length());
            std::string identifier = m[1].str();

            // Append text before the match
            replaced += segment.substr(lastPos, matchStart - lastPos);

            // Check if preceded by '.' (member access) or is an object-literal key
            bool isMemberAccess = (matchStart > 0 && segment[matchStart - 1] == '.');
            bool isObjKey = isObjectLiteralKey(segment, matchStart, matchStart + matchLen);

            if (!isMemberAccess && !isObjKey && nameMap.find(identifier) != nameMap.end()) {
                replaced += nameMap[identifier];
            } else {
                replaced += identifier;
            }

            lastPos = matchStart + matchLen;
            ++it;
        }
        // Append remaining text after last match
        replaced += segment.substr(lastPos);
        result += replaced;
    }

    return result;
}

std::string JSObfuscator::encodeStrings(const std::string& code) {
    std::string result;
    result.reserve(static_cast<size_t>(static_cast<double>(code.size()) * 1.5));  // Encoded strings are longer
    
    bool inString = false;
    bool inSingleQuote = false;
    bool inUnderscoreContext = false;  // Track if we're in a _-prefixed function/variable
    int braceDepth = 0;  // Track nesting depth
    char prevChar = '\0';
    std::string currentString;
    
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        
        // Update context by tracking identifiers and braces
        if (!inString && !inSingleQuote && c == '_' && (i == 0 || (!std::isalnum(static_cast<unsigned char>(code[i-1])) && code[i-1] != '$'))) {
            // Check if this is the start of a _-prefixed identifier
            size_t j = i + 1;
            while (j < code.size() && (std::isalnum(static_cast<unsigned char>(code[j])) || code[j] == '_' || code[j] == '$')) {
                j++;
            }
            if (j > i + 1) {  // Valid identifier starting with _
                // Look ahead to see if followed by ( or = (function or variable)
                while (j < code.size() && std::isspace(static_cast<unsigned char>(code[j]))) j++;
                if (j < code.size() && (code[j] == '(' || code[j] == '=')) {
                    inUnderscoreContext = true;
                    braceDepth = 0;
                }
            }
        }
        
        // Track braces to know when _-context ends
        if (!inString && !inSingleQuote) {
            if (c == '{') {
                braceDepth++;
            } else if (c == '}') {
                braceDepth--;
                if (braceDepth <= 0) {
                    inUnderscoreContext = false;
                    braceDepth = 0;
                }
            }
        }
        
        // Process strings
        if (c == '"' && prevChar != '\\' && !inSingleQuote) {
            if (inString) {
                // End of string - encode it only if NOT in _-context
                if (inUnderscoreContext) {
                    result += '"';
                    result += currentString;
                    result += '"';
                } else {
                    result += encodeStringLiteral(currentString);
                }
                currentString.clear();
                inString = false;
            } else {
                // Start of string
                inString = true;
            }
        } else if (c == '\'' && prevChar != '\\' && !inString) {
            if (inSingleQuote) {
                // End of string - encode it only if NOT in _-context
                if (inUnderscoreContext) {
                    result += '\'';
                    result += currentString;
                    result += '\'';
                } else {
                    // Single quotes also get encoded at level 2
                    result += encodeStringLiteral(currentString);
                }
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
    // Tokenize first so we only touch CODE tokens (skip strings, comments).
    std::vector<Token> tokens = tokenize(code);
    std::regex numberRegex(R"(\b(\d+)\b)");

    std::string result;
    result.reserve(code.size());

    for (const auto& tok : tokens) {
        if (tok.type != TokenType::CODE) {
            // Preserve string literals and comments verbatim
            result += tok.text;
            continue;
        }

        const std::string& segment = tok.text;

        // Collect replaceable integer literal positions within this token
        std::vector<std::pair<size_t, size_t>> positions;
        std::vector<int> numbers;

        auto searchStart = segment.cbegin();
        std::smatch match;
        while (std::regex_search(searchStart, segment.cend(), match, numberRegex)) {
            size_t pos = static_cast<size_t>(std::distance(segment.cbegin(), match[1].first));
            size_t len = match[1].length();
            size_t endPos = pos + len;

            // Skip fractional part of a decimal literal  (e.g. "55" in "0.55")
            bool isDecimalFraction = (pos > 0 && segment[pos - 1] == '.');

            // Skip integer part of a decimal literal  (e.g. "0" in "0.55")
            bool isDecimalInteger = (endPos < segment.size() && segment[endPos] == '.'
                                     && endPos + 1 < segment.size()
                                     && std::isdigit(static_cast<unsigned char>(segment[endPos + 1])));

            if (!isDecimalFraction && !isDecimalInteger) {
                positions.push_back({pos, len});
                numbers.push_back(std::stoi(match[1].str()));
            }

            searchStart = match.suffix().first;
        }

        // Replace from end to start to preserve earlier positions
        std::string tokenResult = segment;
        for (int i = static_cast<int>(positions.size()) - 1; i >= 0; --i) {
            std::string expression = createNumberExpression(numbers[i]);
            tokenResult.replace(positions[i].first, positions[i].second, expression);
        }

        result += tokenResult;
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
    std::vector<Token> tokens = tokenize(code);

    for (const auto& tok : tokens) {
        if (tok.type != TokenType::CODE) continue;

        auto searchStart = tok.text.cbegin();
        std::smatch match;
        while (std::regex_search(searchStart, tok.text.cend(), match, IDENTIFIER_WITH_DOT_REGEX)) {
            std::string dotPrefix = match[1].str();
            std::string identifier = match[2].str();
            if (dotPrefix.empty() && !isReservedKeyword(identifier)) {
                identifiers.push_back(identifier);
            }
            searchStart = match.suffix().first;
        }
    }

    return identifiers;
}

bool JSObfuscator::isReservedKeyword(const std::string& word) {
    return RESERVED_KEYWORDS.find(word) != RESERVED_KEYWORDS.end();
}

std::string JSObfuscator::encodeStringLiteral(const std::string& str) {
    // Encode string to hex escape sequences, but preserve characters that
    // commonly appear in filenames and URL paths so they remain readable.
    // UTF-8 multi-byte characters (ä, ü, ê, etc.) are encoded byte-by-byte.
    std::ostringstream encoded;
    encoded << "\"";
    
    for (char c : str) {
        // Keep alphanumeric, spaces, and filename/path-safe characters as-is
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '/' || c == '.' 
            || c == '-' || c == '_' || c == '~') {
            encoded << c;
        } else {
            // Cast to unsigned char first to ensure proper byte value (0-255)
            unsigned char byte = static_cast<unsigned char>(c);
            encoded << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
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
            // Odd or small – fall back to hex
            {
                std::ostringstream oss;
                oss << "0x" << std::hex << num;
                return oss.str();
            }
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
            // Not a power of 2 – fall back to hex
            {
                std::ostringstream oss;
                oss << "0x" << std::hex << num;
                return oss.str();
            }
        default:
            {
                std::ostringstream oss;
                oss << "0x" << std::hex << num;
                return oss.str();
            }
    }
}

}  // namespace geruest
