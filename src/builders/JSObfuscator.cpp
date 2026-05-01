/**
 * @file JSObfuscator.cpp
 * @date 13.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Implementation of JavaScript obfuscator
 */

#include "JSObfuscator.hpp"
#include "JSObfuscatorScope.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <sys/wait.h>
#include <unistd.h>

namespace geruest {

// JavaScript reserved keywords that should not be mangled
static const std::unordered_set<std::string> RESERVED_KEYWORDS = {
    "abstract", "arguments", "async", "await", "boolean", "break", "byte", "case", "catch",
    "char", "class", "const", "continue", "debugger", "default", "delete", "do",
    "double", "else", "enum", "eval", "export", "extends", "false", "final",
    "finally", "float", "for", "function", "goto", "if", "implements", "import",
    "in", "instanceof", "int", "interface", "let", "long", "native", "new",
    // for-of / for-await-of (contextual keyword; must not be renamed)
    "of",
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
    // Barcode scanner (Quagga2 on window — must match script global spelling)
    "Quagga",
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

/**
 * If code[openSlash] is '/', scan a RegularExpressionLiteral through flags.
 * On success returns true and sets endOut to the index one past the last consumed character (flags).
 */
static bool parseRegexLiteralEnd(const std::string& code, size_t openSlash, size_t& endOut) {
    if (openSlash >= code.size() || code[openSlash] != '/') {
        return false;
    }
    // "//" starts a line comment, not a zero-width RegularExpressionLiteral.
    if (openSlash + 1 < code.size() && code[openSlash + 1] == '/') {
        return false;
    }
    size_t i = openSlash + 1;
    bool inCharClass = false;
    while (i < code.size()) {
        const char rc = code[i];
        if (rc == '\\') {
            i += 2;
            if (i > code.size()) {
                return false;
            }
            continue;
        }
        if (rc == '[') {
            inCharClass = true;
            ++i;
            continue;
        }
        if (rc == ']' && inCharClass) {
            inCharClass = false;
            ++i;
            continue;
        }
        if (rc == '/' && !inCharClass) {
            ++i;
            while (i < code.size() && std::isalpha(static_cast<unsigned char>(code[i]))) {
                ++i;
            }
            endOut = i;
            return true;
        }
        ++i;
    }
    return false;
}

/// True iff code[pos] is '/' and the immediately preceding run of '\\' has odd length (escaped slash).
static bool isEscapedSlashBeforeForTokenize(const std::string& code, size_t pos) {
    if (pos == 0 || code[pos] != '/') {
        return false;
    }
    size_t backslashes = 0;
    for (size_t k = pos; k > 0 && code[k - 1] == '\\'; --k) {
        ++backslashes;
    }
    return (backslashes % 2U) == 1U;
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

/**
 * Find the index of '}' that closes '${' whose expression starts at innerBegin (first char inside { }).
 */
static size_t templateInterpolationClose(const std::string& s, size_t innerBegin) {
    size_t pos = innerBegin;
    int depth = 1;
    bool inDq = false;
    bool inSq = false;
    bool inBt = false;

    while (pos < s.size()) {
        const char c = s[pos];
        if (!inDq && !inSq && !inBt) {
            if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '/') {
                pos += 2;
                while (pos < s.size() && s[pos] != '\n') {
                    ++pos;
                }
                continue;
            }
            if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '*') {
                pos += 2;
                while (pos + 1 < s.size() && !(s[pos] == '*' && s[pos + 1] == '/')) {
                    ++pos;
                }
                pos = (pos + 1 < s.size()) ? pos + 2 : s.size();
                continue;
            }
            if (c == '"') {
                inDq = true;
                ++pos;
                continue;
            }
            if (c == '\'') {
                inSq = true;
                ++pos;
                continue;
            }
            if (c == '`') {
                inBt = true;
                ++pos;
                continue;
            }
            if (c == '{') {
                ++depth;
                ++pos;
                continue;
            }
            if (c == '}') {
                --depth;
                if (depth == 0) {
                    return pos;
                }
                ++pos;
                continue;
            }
            ++pos;
            continue;
        }
        if (inDq) {
            if (c == '\\' && pos + 1 < s.size()) {
                pos += 2;
                continue;
            }
            if (c == '"') {
                inDq = false;
            }
            ++pos;
            continue;
        }
        if (inSq) {
            if (c == '\\' && pos + 1 < s.size()) {
                pos += 2;
                continue;
            }
            if (c == '\'') {
                inSq = false;
            }
            ++pos;
            continue;
        }
        if (inBt) {
            if (c == '\\' && pos + 1 < s.size()) {
                pos += 2;
                continue;
            }
            if (c == '`') {
                inBt = false;
                ++pos;
                continue;
            }
            if (c == '$' && pos + 1 < s.size() && s[pos + 1] == '{') {
                pos += 2;
                const size_t closeInner = templateInterpolationClose(s, pos);
                if (closeInner == std::string::npos) {
                    return std::string::npos;
                }
                pos = closeInner + 1;
                continue;
            }
            ++pos;
            continue;
        }
    }
    return std::string::npos;
}

static size_t tryConsumeNestedTemplateLiteralObf(const std::string& code, size_t openTick) {
    if (openTick >= code.size() || code[openTick] != '`') {
        return std::string::npos;
    }
    size_t pos = openTick + 1;
    const size_t n = code.size();
    while (pos < n) {
        if (code[pos] == '\\' && pos + 1 < n) {
            pos += 2;
            continue;
        }
        if (code[pos] == '`') {
            return pos + 1;
        }
        if (code[pos] == '$' && pos + 1 < n && code[pos + 1] == '{') {
            pos += 2;
            const size_t close = templateInterpolationClose(code, pos);
            if (close == std::string::npos) {
                return std::string::npos;
            }
            pos = close + 1;
            continue;
        }
        ++pos;
    }
    return std::string::npos;
}

static bool templateBacktickLooksLikeTerminatorObf(const std::string& code, size_t tickPos) {
    size_t j = tickPos + 1;
    const size_t n = code.size();
    while (j < n && (code[j] == ' ' || code[j] == '\t')) {
        ++j;
    }
    if (j >= n) {
        return true;
    }
    const char t = code[j];
    if (t == ';' || t == ',' || t == ')' || t == '}' || t == ']' || t == ':') {
        return true;
    }
    if (t == '\n' || t == '\r') {
        while (j < n && (code[j] == '\n' || code[j] == '\r' || code[j] == ' ' || code[j] == '\t')) {
            ++j;
        }
        if (j < n) {
            static const char* const tops[] = {"function", "const", "let", "var", "class", "async",
                                                 "export", "import", "return", "throw", "debugger"};
            for (const char* kw : tops) {
                const size_t L = std::strlen(kw);
                if (j + L <= n && code.compare(j, L, kw) == 0) {
                    const char nx = code[j + L];
                    const bool part =
                        std::isalnum(static_cast<unsigned char>(nx)) || nx == '_' || nx == '$';
                    if (j + L >= n || !part) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static size_t lexTemplateLiteralEndObf(const std::string& code, size_t openTick) {
    if (openTick >= code.size() || code[openTick] != '`') {
        return openTick + 1;
    }
    size_t i = openTick + 1;
    const size_t n = code.size();
    while (i < n) {
        if (code[i] == '\\' && i + 1 < n) {
            i += 2;
            continue;
        }
        if (code[i] == '`') {
            if (i + 1 < n && code[i + 1] == '`') {
                i += 2;
                continue;
            }
            if (templateBacktickLooksLikeTerminatorObf(code, i)) {
                return i + 1;
            }
            const size_t nestedEnd = tryConsumeNestedTemplateLiteralObf(code, i);
            if (nestedEnd != std::string::npos) {
                i = nestedEnd;
                continue;
            }
            return i + 1;
        }
        if (code[i] == '$' && i + 1 < n && code[i + 1] == '{') {
            i += 2;
            const size_t close = templateInterpolationClose(code, i);
            if (close == std::string::npos) {
                return std::string::npos;
            }
            i = close + 1;
            continue;
        }
        ++i;
    }
    return std::string::npos;
}

/// From opening '`', return index past closing '`' (handles `${...}` and nested templates).
static size_t skipTemplateLiteral(const std::string& code, size_t openTick) {
    size_t end = lexTemplateLiteralEndObf(code, openTick);
    if (end == std::string::npos) {
        return code.size();
    }
    return end;
}

/** Raw string between quotes (excluding delimiters), honoring backslash escapes. */
static std::string extractQuotedInner(const std::string& tok) {
    if (tok.size() < 2) {
        return "";
    }
    const char q = tok[0];
    std::string inner;
    size_t i = 1;
    while (i < tok.size()) {
        if (tok[i] == '\\' && i + 1 < tok.size()) {
            inner += tok[i];
            inner += tok[i + 1];
            i += 2;
            continue;
        }
        if (tok[i] == q) {
            break;
        }
        inner += tok[i];
        ++i;
    }
    return inner;
}

JSObfuscator::JSObfuscator(unsigned int level)
    : JSObfuscator(level, JSObfuscateSettings{}) {
}

JSObfuscator::JSObfuscator(unsigned int level, JSObfuscateSettings settings)
    : _level(level), _settings(std::move(settings)) {
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    _rng.seed(static_cast<unsigned int>(seed));
}

std::string JSObfuscator::obfuscate(const std::string& code) {
    if (_level == 0 || code.empty()) {
        return code;
    }

    _lastDiagnostics.clear();
    _lastTopLevelPreserved.clear();

    std::string result = code;

    if (_level >= 1) {
        result = mangleNames(result);
        result = removeWhitespace(result);
    }

    if (_level >= 2) {
        result = encodeStrings(result);
        result = obfuscateNumbers(result);
    }

    if (_level >= 3) {
        result = injectDeadCode(result);
        result = obfuscateControlFlow(result);
    }

    if (_level >= 1 && _settings.emitGlobalThisAssignments && !_lastTopLevelPreserved.empty()) {
        std::sort(_lastTopLevelPreserved.begin(), _lastTopLevelPreserved.end());
        _lastTopLevelPreserved.erase(
            std::unique(_lastTopLevelPreserved.begin(), _lastTopLevelPreserved.end()),
            _lastTopLevelPreserved.end());
        for (const std::string& n : _lastTopLevelPreserved) {
            std::string esc;
            esc.reserve(n.size());
            for (char c : n) {
                if (c == '\\' || c == '\'' || c == '\"') {
                    esc += '\\';
                }
                esc += c;
            }
            result += "\nglobalThis['";
            result += esc;
            result += "']=";
            result += n;
            result += ';';
        }
    }

    if (_settings.validateOutputWithAcorn) {
        if (!tryValidateWithAcorn(result)) {
            _lastDiagnostics.push_back(
                "Acorn validation failed or node/acorn not available (output still returned)");
        }
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
    char prevChar = '\0';
    
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        
        // Track string literals
        if (c == '"' && prevChar != '\\' && !inSingleQuote) {
            inString = !inString;
            result += c;
        } else if (c == '\'' && prevChar != '\\' && !inString) {
            inSingleQuote = !inSingleQuote;
            result += c;
        } else if (c == '`' && prevChar != '\\' && !inString && !inSingleQuote) {
            size_t end = skipTemplateLiteral(code, i);
            result.append(code, i, end - i);
            if (end > i) {
                prevChar = code[end - 1];
            } else {
                prevChar = c;
            }
            i = end - 1;
            continue;
        } else if (inString || inSingleQuote) {
            // Inside string literals, preserve everything
            result += c;
        } else if (c == '/' && i + 1 < code.size()) {
            // Block comments — copy verbatim so internals are not rescanned as code.
            if (code[i + 1] == '*') {
                size_t start = i;
                i += 2;
                bool closed = false;
                while (i + 1 < code.size()) {
                    if (code[i] == '*' && code[i + 1] == '/') {
                        i += 2;
                        closed = true;
                        break;
                    }
                    ++i;
                }
                if (!closed) {
                    i = code.size();
                }
                result.append(code, start, i - start);
                if (i > start) {
                    prevChar = code[i - 1];
                }
                --i;
                continue;
            }
            // Regex before // so /^\// and /\/\// are not mistaken for line comments.
            if (canStartRegexLiteral(code, i)) {
                size_t end = 0;
                if (parseRegexLiteralEnd(code, i, end)) {
                    result.append(code, i, end - i);
                    prevChar = code[end - 1];
                    i = end - 1;
                    continue;
                }
            }
            // Line comments must include the terminating LineTerminator. Otherwise
            // removeWhitespace can drop the newline and merge the next statement onto
            // the same line as the //, swallowing it as comment text (syntax error).
            if (code[i + 1] == '/' && !isEscapedSlashBeforeForTokenize(code, i)) {
                size_t start = i;
                i += 2;
                while (i < code.size() && code[i] != '\n' && code[i] != '\r') {
                    ++i;
                }
                if (i < code.size()) {
                    if (code[i] == '\r' && i + 1 < code.size() && code[i + 1] == '\n') {
                        i += 2;
                    } else {
                        ++i;
                    }
                }
                result.append(code, start, i - start);
                if (i > start) {
                    prevChar = code[i - 1];
                }
                --i;
                continue;
            }
            result += c;
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

        // --- template literal (must handle nested `...` and `${}`; do not stop at inner backtick) ---
        if (c == '`') {
            flushCode();
            const size_t tplStart = i;
            const size_t tplEnd = skipTemplateLiteral(code, i);
            tokens.push_back({TokenType::STRING_LITERAL, code.substr(tplStart, tplEnd - tplStart)});
            i = tplEnd;
            continue;
        }

        // --- string literal (double-quote, single-quote) ---
        if (c == '"' || c == '\'') {
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
        // If parseRegexLiteralEnd fails (e.g. "//" line comment after ';'), fall through to
        // the '//' line-comment rule — do not consume only the first '/' (that strands the
        // second '/' as code and breaks comments containing apostrophes: 'doesn't').
        if (c == '/' && canStartRegexLiteral(code, i)) {
            flushCode();
            size_t regexStart = i;
            size_t end = 0;
            if (parseRegexLiteralEnd(code, i, end)) {
                tokens.push_back({TokenType::REGEX_LITERAL, code.substr(regexStart, end - regexStart)});
                i = end;
                continue;
            }
        }

        // --- line comment (after regex attempt; parseRegexLiteralEnd rejects "//") ---
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/' &&
            !isEscapedSlashBeforeForTokenize(code, i)) {
            flushCode();
            std::string comment;
            while (i < code.size() && code[i] != '\n') {
                comment += code[i++];
            }
            if (i < code.size()) comment += code[i++]; // include the newline
            tokens.push_back({TokenType::LINE_COMMENT, comment});
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

std::string JSObfuscator::mangleNames(const std::string& code) {
    _lastTopLevelPreserved.clear();

    js_scope::ScopeRenameOptions opt;
    opt.reserved = &RESERVED_KEYWORDS;
    opt.preserve = _settings.preserveIdentNames;
    opt.externNames = _settings.externGlobalNames;
    opt.generateMangledName = [this]() { return generateRandomName(6); };
    opt.strictFreeIdentifiers = _settings.strictUndefinedSymbols;
    opt.autoPreserveBracketStringKeys = _settings.autoPreserveBracketStringKeys;
    std::vector<std::string>* topPreservePtr =
        _settings.emitGlobalThisAssignments ? &_lastTopLevelPreserved : nullptr;

    js_scope::ScopeRenamePlan plan = js_scope::computeScopedRenames(code, opt, topPreservePtr);

    if (plan.usedLegacyFallback) {
        _lastDiagnostics.push_back("obfuscator: spelling-keyed rename fallback was used");
    }
    for (const auto& u : plan.undefinedSymbols) {
        _lastDiagnostics.push_back(std::string("obfuscator: undefined symbol reference: ") + u);
    }
    if (_settings.strictUndefinedSymbols && !plan.undefinedSymbols.empty()) {
        std::string msg = "JS obfuscation: strict undefined symbols (declare, add extern, or fix): ";
        for (size_t k = 0; k < plan.undefinedSymbols.size(); ++k) {
            if (k > 0) {
                msg += ", ";
            }
            msg += plan.undefinedSymbols[k];
        }
        throw std::runtime_error(msg);
    }

    std::string out = code;
    std::vector<js_scope::RenameSpan> spans = plan.spans;
    std::sort(spans.begin(), spans.end(), [](const js_scope::RenameSpan& a, const js_scope::RenameSpan& b) {
        return a.start > b.start;
    });
    for (const auto& s : spans) {
        if (s.start > out.size() || s.end > out.size() || s.start >= s.end) {
            continue;
        }
        out.replace(s.start, s.end - s.start, s.mangled);
    }
    return out;
}

bool JSObfuscator::tryValidateWithAcorn(const std::string& js) const {
    static const char kSnippet[] =
        "let d='';process.stdin.on('data',c=>d+=c);process.stdin.on('end',()=>{"
        "try{require('acorn').parse(d,{ecmaVersion:2022,allowReturnOutsideFunction:true});"
        "process.exit(0);}catch(e){process.stderr.write(String(e)+'\\n');process.exit(1);}"
        "});";
    char tmpPath[] = "/tmp/geruest_acorn_chkXXXXXX";
    int tfd = mkstemp(tmpPath);
    if (tfd < 0) {
        return false;
    }
    {
        const ssize_t n = static_cast<ssize_t>(sizeof(kSnippet) - 1);
        if (write(tfd, kSnippet, static_cast<size_t>(n)) != n) {
            close(tfd);
            unlink(tmpPath);
            return false;
        }
        close(tfd);
    }
    std::string cmd = std::string("node \"") + tmpPath + "\"";
    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) {
        unlink(tmpPath);
        return false;
    }
    if (!js.empty()) {
        size_t w = fwrite(js.data(), 1, js.size(), pipe);
        (void)w;
    }
    int st = pclose(pipe);
    unlink(tmpPath);
    if (st == -1) {
        return false;
    }
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

std::string JSObfuscator::encodeStrings(const std::string& code) {
    const std::vector<Token> tokens = tokenize(code);
    std::string result;
    result.reserve(static_cast<size_t>(static_cast<double>(code.size()) * 1.5));

    for (const auto& tok : tokens) {
        if (tok.type == TokenType::CODE) {
            result += tok.text;
            continue;
        }
        if (tok.type == TokenType::STRING_LITERAL) {
            if (!tok.text.empty() && tok.text[0] == '`') {
                result += tok.text;
                continue;
            }
            if (tok.text.size() >= 2) {
                const char q = tok.text[0];
                if (q == '"' || q == '\'') {
                    const std::string inner = extractQuotedInner(tok.text);
                    result += encodeStringLiteral(inner);
                    continue;
                }
            }
            result += tok.text;
            continue;
        }
        result += tok.text;
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
    // Encode string to escape sequences.
    // ASCII special chars use \xHH; multi-byte UTF-8 sequences are decoded to
    // their Unicode codepoint and emitted as \uXXXX (or a surrogate pair for
    // codepoints above U+FFFF), because JavaScript \xHH is Latin-1 — treating
    // raw UTF-8 bytes as \xHH produces mojibake (e.g. Ã instead of Ü).
    std::ostringstream encoded;
    encoded << "\"";

    size_t i = 0;
    while (i < str.size()) {
        unsigned char byte = static_cast<unsigned char>(str[i]);

        // Keep alphanumeric, spaces, and filename/path-safe ASCII as-is
        if (std::isalnum(byte) || byte == ' ' || byte == '/' || byte == '.'
            || byte == '-' || byte == '_' || byte == '~') {
            encoded << str[i];
            ++i;
        } else if (byte < 0x80) {
            // Single-byte ASCII special character → \xHH
            encoded << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            ++i;
        } else {
            // Multi-byte UTF-8 sequence: decode to Unicode codepoint → \uXXXX
            uint32_t codepoint = 0;
            size_t extraBytes = 0;

            if ((byte & 0xE0) == 0xC0) {        // 110xxxxx – 2-byte sequence
                codepoint = byte & 0x1F;
                extraBytes = 1;
            } else if ((byte & 0xF0) == 0xE0) { // 1110xxxx – 3-byte sequence
                codepoint = byte & 0x0F;
                extraBytes = 2;
            } else if ((byte & 0xF8) == 0xF0) { // 11110xxx – 4-byte sequence
                codepoint = byte & 0x07;
                extraBytes = 3;
            } else {
                // Invalid UTF-8 lead byte – emit raw \xHH and move on
                encoded << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                ++i;
                continue;
            }

            ++i;
            for (size_t j = 0; j < extraBytes && i < str.size(); ++j, ++i) {
                unsigned char cont = static_cast<unsigned char>(str[i]);
                if ((cont & 0xC0) != 0x80) {
                    break;  // Invalid continuation byte – stop consuming
                }
                codepoint = (codepoint << 6) | (cont & 0x3F);
            }

            if (codepoint <= 0xFFFF) {
                encoded << "\\u" << std::hex << std::setw(4) << std::setfill('0') << codepoint;
            } else {
                // Encode as a UTF-16 surrogate pair for codepoints above U+FFFF
                codepoint -= 0x10000;
                uint32_t high = 0xD800 + (codepoint >> 10);
                uint32_t low  = 0xDC00 + (codepoint & 0x3FF);
                encoded << "\\u" << std::hex << std::setw(4) << std::setfill('0') << high;
                encoded << "\\u" << std::hex << std::setw(4) << std::setfill('0') << low;
            }
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
