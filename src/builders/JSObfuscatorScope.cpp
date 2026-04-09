/**
 * @file JSObfuscatorScope.cpp
 * @brief Lexical scope analysis for JavaScript renaming (var hoisting, let/const blocks, functions).
 */

#include "JSObfuscatorScope.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace geruest::js_scope {

namespace {

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
    static const std::unordered_set<std::string> regexKeywords = {
        "return", "throw", "case", "delete", "void", "typeof", "instanceof", "in", "of"
    };
    return regexKeywords.find(word) != regexKeywords.end();
}

static bool parseRegexLiteralEnd(const std::string& code, size_t openSlash, size_t& endOut) {
    if (openSlash >= code.size() || code[openSlash] != '/') {
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

static bool canStartRegexLiteral(const std::string& code, size_t slashPos) {
    int j = static_cast<int>(slashPos) - 1;
    while (j >= 0 && std::isspace(static_cast<unsigned char>(code[static_cast<size_t>(j)]))) {
        --j;
    }
    if (j < 0) {
        return true;
    }
    const char prev = code[static_cast<size_t>(j)];
    if ((prev == '+' || prev == '-') && j > 0) {
        char beforePrev = code[static_cast<size_t>(j - 1)];
        if (beforePrev == prev) {
            return false;
        }
    }
    if (isRegexAllowedAfterChar(prev)) {
        return true;
    }
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

/// If code[openTick] is '`' and a well-formed nested template literal starts here, returns the
/// index past its closing '`'. Otherwise npos (caller treats that '`' as closing the current level).
static size_t tryConsumeNestedTemplateLiteral(const std::string& code, size_t openTick) {
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

/// From opening '`', return index past closing '`' (handles `${...}` and nested templates).
static size_t lexTemplateLiteralEnd(const std::string& code, size_t openTick) {
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
            const size_t afterNested = tryConsumeNestedTemplateLiteral(code, i);
            if (afterNested != std::string::npos) {
                i = afterNested;
                continue;
            }
            ++i;
            return i;
        }
        if (code[i] == '$' && i + 1 < n && code[i + 1] == '{') {
            i += 2;
            const size_t close = templateInterpolationClose(code, i);
            if (close == std::string::npos) {
                return n;
            }
            i = close + 1;
            continue;
        }
        ++i;
    }
    return n;
}

enum class Tk { Ident, Num, Str, Tpl, Rx, Pun, Eof };

struct Tok {
    Tk kind = Tk::Eof;
    size_t a = 0;
    size_t b = 0;
};

static bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

static bool isIdentPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

static bool isObjectLiteralKey(const std::string& segment, size_t identStart, size_t identEnd) {
    bool followedByColon = false;
    for (size_t k = identEnd; k < segment.size(); ++k) {
        char c = segment[k];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        followedByColon = (c == ':');
        break;
    }
    if (!followedByColon) {
        return false;
    }
    for (int j = static_cast<int>(identStart) - 1; j >= 0; --j) {
        char c = segment[static_cast<size_t>(j)];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        return (c == '{' || c == ',');
    }
    return false;
}

static std::string tokTxt(const std::string& code, const Tok& t) {
    return code.substr(t.a, t.b - t.a);
}

static void lexAll(const std::string& code, std::vector<Tok>& out) {
    size_t i = 0;
    const size_t n = code.size();

    while (i < n) {
        unsigned char uc = static_cast<unsigned char>(code[i]);
        if (std::isspace(uc)) {
            ++i;
            continue;
        }

        if (i + 1 < n && code[i] == '/' && code[i + 1] == '/') {
            while (i < n && code[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (i + 1 < n && code[i] == '/' && code[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(code[i] == '*' && code[i + 1] == '/')) {
                ++i;
            }
            if (i + 1 < n) {
                i += 2;
            }
            continue;
        }

        if (code[i] == '"' || code[i] == '\'') {
            const size_t start = i;
            char q = code[i++];
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n) {
                    i += 2;
                    continue;
                }
                if (code[i] == q) {
                    ++i;
                    break;
                }
                ++i;
            }
            out.push_back({Tk::Str, start, i});
            continue;
        }

        if (code[i] == '`') {
            const size_t start = i;
            i = lexTemplateLiteralEnd(code, i);
            out.push_back({Tk::Tpl, start, i});
            continue;
        }

        if (code[i] == '/' && canStartRegexLiteral(code, i)) {
            size_t end = 0;
            const size_t start = i;
            if (parseRegexLiteralEnd(code, i, end)) {
                i = end;
                out.push_back({Tk::Rx, start, end});
            } else {
                out.push_back({Tk::Pun, i, i + 1});
                ++i;
            }
            continue;
        }

        if (isIdentStart(code[i])) {
            const size_t start = i;
            ++i;
            while (i < n && isIdentPart(code[i])) {
                ++i;
            }
            out.push_back({Tk::Ident, start, i});
            continue;
        }

        if (std::isdigit(uc) || (code[i] == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(code[i + 1])))) {
            const size_t start = i;
            if (code[i] == '0' && i + 1 < n && (code[i + 1] == 'x' || code[i + 1] == 'X')) {
                i += 2;
                while (i < n && std::isxdigit(static_cast<unsigned char>(code[i]))) {
                    ++i;
                }
            } else {
                while (i < n && (std::isdigit(static_cast<unsigned char>(code[i])) || code[i] == '.')) {
                    ++i;
                }
                if (i < n && (code[i] == 'e' || code[i] == 'E')) {
                    ++i;
                    if (i < n && (code[i] == '+' || code[i] == '-')) {
                        ++i;
                    }
                    while (i < n && std::isdigit(static_cast<unsigned char>(code[i]))) {
                        ++i;
                    }
                }
            }
            out.push_back({Tk::Num, start, i});
            continue;
        }

        out.push_back({Tk::Pun, i, i + 1});
        ++i;
    }
    out.push_back({Tk::Eof, n, n});
}

struct Env {
    Env* parentFn = nullptr;
    std::unordered_map<std::string, int> hoisted;
    std::vector<std::unordered_map<std::string, int>> blockStack;
};

struct Parser {
    const std::string* code = nullptr;
    const std::vector<Tok>* tok = nullptr;
    size_t i = 0;

    ScopeRenameOptions opt;
    std::vector<std::pair<std::string, std::string>> bindMangled;
    std::vector<RenameSpan> spans;
    std::vector<std::string> undefinedSyms;

    Env rootEnv;
    Env* curEnv = nullptr;
    /// Heap-allocated function Envs so curEnv/parentFn stay valid when this vector grows.
    std::vector<std::unique_ptr<Env>> fnPool;
    int nextBind = 0;

    std::vector<std::string>* topLevelPreservedOut = nullptr;
    bool atScriptTopLevel = true;

    /// Spelling-keyed bindings for implicit globals (compat with pre-scope obfuscator).
    std::unordered_map<std::string, int> implicitGlobalSpellingToBid;
    /// Ensure distinct mangled spellings for distinct bindings (avoid RNG collisions).
    std::unordered_set<std::string> usedMangledOutput;

    bool isReservedWord(const std::string& w) const {
        return opt.reserved && opt.reserved->find(w) != opt.reserved->end();
    }

    bool shouldPreserve(const std::string& w) const {
        return opt.preserve.find(w) != opt.preserve.end();
    }

    bool isExtern(const std::string& w) const {
        return opt.externNames.find(w) != opt.externNames.end();
    }

    int makeBinding(const std::string& orig) {
        int id = nextBind++;
        std::string mang;
        if (shouldPreserve(orig) || !opt.generateMangledName) {
            mang = orig;
        } else {
            for (int attempt = 0; attempt < 256; ++attempt) {
                mang = opt.generateMangledName();
                if (mang.empty()) {
                    mang = orig;
                }
                if (usedMangledOutput.find(mang) == usedMangledOutput.end()) {
                    break;
                }
            }
            if (usedMangledOutput.find(mang) != usedMangledOutput.end()) {
                mang = orig + std::string("_g") + std::to_string(id);
            }
        }
        usedMangledOutput.insert(mang);
        if (static_cast<size_t>(id) >= bindMangled.size()) {
            bindMangled.resize(static_cast<size_t>(id) + 1);
        }
        bindMangled[static_cast<size_t>(id)] = {orig, mang};
        return id;
    }

    int implicitGlobalBinding(const std::string& name) {
        auto it = implicitGlobalSpellingToBid.find(name);
        if (it != implicitGlobalSpellingToBid.end()) {
            return it->second;
        }
        int bid = makeBinding(name);
        implicitGlobalSpellingToBid[name] = bid;
        return bid;
    }

    /// Hoisted declarations (function name, var) may appear after skip regions referenced the same
    /// spelling as a free name at script level — reuse that binding (JS hoisting). Nested scopes use
    /// a separate Env; do not reuse file-level implicits or inner locals could alias globals.
    int bidForHoistedDecl(const std::string& name) {
        if (curEnv == &rootEnv) {
            auto ig = implicitGlobalSpellingToBid.find(name);
            if (ig != implicitGlobalSpellingToBid.end()) {
                return ig->second;
            }
        }
        return makeBinding(name);
    }

    std::optional<int> lookup(const std::string& name) const {
        for (const Env* e = curEnv; e != nullptr; e = e->parentFn) {
            for (auto it = e->blockStack.rbegin(); it != e->blockStack.rend(); ++it) {
                auto f = it->find(name);
                if (f != it->end()) {
                    return f->second;
                }
            }
            auto hf = e->hoisted.find(name);
            if (hf != e->hoisted.end()) {
                return hf->second;
            }
        }
        return std::nullopt;
    }

    void recordSpan(size_t a, size_t b, const std::string& mang) {
        if (a < b) {
            spans.push_back({a, b, mang});
        }
    }

    /// Scan template literal token [tplA, tplB) for ${ ... } and resolve idents against current Env.
    void scanTemplateToken(size_t tplA, size_t tplB) {
        if (tplA >= tplB || tplB > code->size()) {
            return;
        }
        const std::string& full = *code;
        if (tplA >= full.size() || full[tplA] != '`') {
            return;
        }
        size_t j = tplA + 1;
        while (j < tplB) {
            if (full[j] == '\\' && j + 1 < tplB) {
                j += 2;
                continue;
            }
            if (full[j] == '`') {
                break;
            }
            if (full[j] == '$' && j + 1 < full.size() && full[j + 1] == '{') {
                size_t inner = j + 2;
                size_t close = templateInterpolationClose(full, inner);
                if (close == std::string::npos || close > tplB) {
                    break;
                }
                std::vector<Tok> sub;
                lexAll(full.substr(inner, close - inner), sub);
                for (const Tok& tt : sub) {
                    if (tt.kind == Tk::Ident) {
                        referenceIdent(inner + tt.a, inner + tt.b);
                    } else if (tt.kind == Tk::Tpl) {
                        scanTemplateToken(inner + tt.a, inner + tt.b);
                    }
                }
                j = close + 1;
                continue;
            }
            ++j;
        }
    }

    void declareHoisted(const std::string& name, size_t a, size_t b) {
        if (name.empty() || name[0] == '_' || isReservedWord(name)) {
            return;
        }
        auto ex = curEnv->hoisted.find(name);
        if (ex != curEnv->hoisted.end()) {
            recordSpan(a, b, bindMangled[static_cast<size_t>(ex->second)].second);
            if (topLevelPreservedOut && atScriptTopLevel && shouldPreserve(name)) {
                topLevelPreservedOut->push_back(name);
            }
            return;
        }
        int bid = bidForHoistedDecl(name);
        curEnv->hoisted[name] = bid;
        recordSpan(a, b, bindMangled[static_cast<size_t>(bid)].second);
        if (topLevelPreservedOut && atScriptTopLevel && shouldPreserve(name)) {
            topLevelPreservedOut->push_back(name);
        }
    }

    void declareLet(const std::string& name, size_t a, size_t b) {
        if (name.empty() || name[0] == '_' || isReservedWord(name)) {
            return;
        }
        if (curEnv->blockStack.empty()) {
            curEnv->blockStack.push_back({});
        }
        auto& top = curEnv->blockStack.back();
        auto ex = top.find(name);
        if (ex != top.end()) {
            recordSpan(a, b, bindMangled[static_cast<size_t>(ex->second)].second);
            return;
        }
        int bid = makeBinding(name);
        top[name] = bid;
        recordSpan(a, b, bindMangled[static_cast<size_t>(bid)].second);
    }

    void referenceIdent(size_t a, size_t b) {
        std::string name = code->substr(a, b - a);
        if (name.empty() || name[0] == '_' || isReservedWord(name)) {
            return;
        }
        size_t p = a;
        while (p > 0 && std::isspace(static_cast<unsigned char>((*code)[p - 1]))) {
            --p;
        }
        if (p > 0 && (*code)[p - 1] == '.') {
            return;
        }
        if (p > 0 && (*code)[p - 1] == '?' && p >= 2 && (*code)[p - 2] == '.') {
            return;
        }
        if (isObjectLiteralKey(*code, a, b)) {
            return;
        }
        auto bid = lookup(name);
        if (bid) {
            recordSpan(a, b, bindMangled[static_cast<size_t>(*bid)].second);
            return;
        }
        if (isExtern(name)) {
            recordSpan(a, b, name);
            return;
        }
        if (opt.strictFreeIdentifiers) {
            undefinedSyms.push_back(name);
            recordSpan(a, b, name);
            return;
        }
        int ig = implicitGlobalBinding(name);
        recordSpan(a, b, bindMangled[static_cast<size_t>(ig)].second);
    }

    bool eof() const {
        return i >= tok->size() || (*tok)[i].kind == Tk::Eof;
    }

    char punChar() const {
        if (eof()) {
            return '\0';
        }
        const Tok& t = (*tok)[i];
        if (t.kind != Tk::Pun || t.a >= code->size()) {
            return '\0';
        }
        return (*code)[t.a];
    }

    bool tryPunct(char c) {
        if (punChar() == c) {
            ++i;
            return true;
        }
        return false;
    }

    void mustPunct(char c) {
        if (!tryPunct(c)) {
            /* best-effort recovery */
        }
    }

    std::string peekIdentStr() const {
        if (eof() || (*tok)[i].kind != Tk::Ident) {
            return "";
        }
        return tokTxt(*code, (*tok)[i]);
    }

    bool matchKeyword(const char* kw) {
        if (peekIdentStr() != kw) {
            return false;
        }
        ++i;
        return true;
    }

    void skipBalanced(char open, char close) {
        int d = 0;
        if (punChar() == open) {
            d = 1;
            ++i;
        }
        while (!eof() && d > 0) {
            const Tok& t = (*tok)[i];
            if (t.kind == Tk::Pun) {
                char c = (*code)[t.a];
                if (c == open) {
                    ++d;
                } else if (c == close) {
                    --d;
                }
                ++i;
                if (d == 0) {
                    break;
                }
            } else if (t.kind == Tk::Ident) {
                referenceIdent(t.a, t.b);
                ++i;
            } else if (t.kind == Tk::Tpl) {
                scanTemplateToken(t.a, t.b);
                ++i;
            } else {
                ++i;
            }
        }
    }

    /// Skip until `term` at depth 0 of () [] {}
    void skipUntilTerminal(char term) {
        int p = 0;
        int b = 0;
        int c = 0;
        /// True after `}` that closed the innermost block (c: 1→0) while still inside a call (p>0).
        /// If the next punctuator is `)` that brings p to 0, end the statement without requiring `;`
        /// (ASI after `});` for addEventListener(..., function () { ... }) ).
        bool expectCallCloseAfterBlock = false;
        while (!eof()) {
            const Tok& t = (*tok)[i];
            if (t.kind == Tk::Ident) {
                expectCallCloseAfterBlock = false;
                referenceIdent(t.a, t.b);
                ++i;
                continue;
            }
            if (t.kind == Tk::Tpl) {
                expectCallCloseAfterBlock = false;
                scanTemplateToken(t.a, t.b);
                ++i;
                continue;
            }
            if (t.kind != Tk::Pun) {
                expectCallCloseAfterBlock = false;
                ++i;
                continue;
            }
            char ch = (*code)[t.a];
            if (ch == '(') {
                expectCallCloseAfterBlock = false;
                ++p;
            } else if (ch == ')') {
                if (p > 0) {
                    --p;
                }
                if (expectCallCloseAfterBlock && p <= 0 && b <= 0 && c <= 0) {
                    expectCallCloseAfterBlock = false;
                    ++i;
                    // (function () { })(…) — closing `)` is followed by `(`; keep scanning.
                    if (!eof() && (*tok)[i].kind == Tk::Pun && (*code)[(*tok)[i].a] == '(') {
                        continue;
                    }
                    return;
                }
                expectCallCloseAfterBlock = false;
            } else if (ch == '[') {
                expectCallCloseAfterBlock = false;
                ++b;
            } else if (ch == ']') {
                expectCallCloseAfterBlock = false;
                if (b > 0) {
                    --b;
                }
            } else if (ch == '{') {
                expectCallCloseAfterBlock = false;
                ++c;
            } else if (ch == '}') {
                if (c > 0) {
                    if (c == 1 && p > 0) {
                        expectCallCloseAfterBlock = true;
                    } else {
                        expectCallCloseAfterBlock = false;
                    }
                    --c;
                } else {
                    // `}` closes a block we did not open in this skip (e.g. function body `}`
                    // after `return …;`). Stop here without consuming — let parseStatementList
                    // / tryPunct('}') handle it. Consuming would strand the parser inside the fn.
                    expectCallCloseAfterBlock = false;
                    return;
                }
            } else {
                expectCallCloseAfterBlock = false;
            }
            if (p <= 0 && b <= 0 && c <= 0 && ch == term) {
                ++i;
                return;
            }
            ++i;
        }
    }

    void parseVarList() {
        while (!eof()) {
            const Tok& t = (*tok)[i];
            if (t.kind != Tk::Ident) {
                ++i;
                if (punChar() == ';' || punChar() == ')') {
                    return;
                }
                continue;
            }
            std::string n = tokTxt(*code, t);
            size_t a = t.a;
            size_t bb = t.b;
            ++i;
            auto ex = curEnv->hoisted.find(n);
            if (ex != curEnv->hoisted.end()) {
                recordSpan(a, bb, bindMangled[static_cast<size_t>(ex->second)].second);
            } else {
                int bid = bidForHoistedDecl(n);
                curEnv->hoisted[n] = bid;
                recordSpan(a, bb, bindMangled[static_cast<size_t>(bid)].second);
            }
            if (tryPunct('=')) {
                skipUntilTerminal(';');
                return;
            }
            if (tryPunct(',')) {
                continue;
            }
            return;
        }
    }

    void parseLetList() {
        while (!eof()) {
            const Tok& t = (*tok)[i];
            if (t.kind != Tk::Ident) {
                ++i;
                if (punChar() == ';' || punChar() == ')') {
                    return;
                }
                continue;
            }
            std::string n = tokTxt(*code, t);
            declareLet(n, t.a, t.b);
            ++i;
            if (tryPunct('=')) {
                skipUntilTerminal(';');
                return;
            }
            if (tryPunct(',')) {
                continue;
            }
            return;
        }
    }

    void enterBlock() {
        if (curEnv->blockStack.empty()) {
            curEnv->blockStack.push_back({});
        }
        curEnv->blockStack.push_back({});
    }

    void exitBlock() {
        if (curEnv->blockStack.size() > 1) {
            curEnv->blockStack.pop_back();
        }
    }

    void parseStatementListUntilRBrace() {
        while (!eof()) {
            if (punChar() == '}') {
                return;
            }
            parseStatement();
        }
    }

    void parseBlock() {
        mustPunct('{');
        enterBlock();
        parseStatementListUntilRBrace();
        tryPunct('}');
        exitBlock();
    }

    void parseFunctionAfterKeyword(bool isDecl) {
        (void)isDecl;
        std::string fname;
        size_t fa = 0;
        size_t fb = 0;
        if ((*tok)[i].kind == Tk::Ident) {
            fname = tokTxt(*code, (*tok)[i]);
            fa = (*tok)[i].a;
            fb = (*tok)[i].b;
            ++i;
            declareHoisted(fname, fa, fb);
        }
        tryPunct('*');
        std::vector<std::pair<size_t, size_t>> paramRanges;
        if (tryPunct('(')) {
            while (!eof() && punChar() != ')') {
                if ((*tok)[i].kind == Tk::Ident) {
                    paramRanges.push_back({(*tok)[i].a, (*tok)[i].b});
                    ++i;
                } else {
                    skipBalanced('(', ')');
                }
                if (tryPunct('=')) {
                    skipUntilTerminal(',');
                }
                if (tryPunct(',')) {
                    continue;
                }
                break;
            }
            tryPunct(')');
        }
        bool savedTop = atScriptTopLevel;
        atScriptTopLevel = false;

        fnPool.push_back(std::make_unique<Env>());
        Env* ne = fnPool.back().get();
        ne->parentFn = curEnv;
        ne->hoisted.clear();
        ne->blockStack.clear();
        ne->blockStack.push_back({});
        Env* saved = curEnv;
        curEnv = ne;
        for (const auto& pr : paramRanges) {
            declareLet(tokTxt(*code, {Tk::Ident, pr.first, pr.second}), pr.first, pr.second);
        }
        if (tryPunct('{')) {
            parseStatementListUntilRBrace();
            tryPunct('}');
        }
        curEnv = saved;
        atScriptTopLevel = savedTop;
    }

    void parseForHeader() {
        tryPunct('(');
        if (matchKeyword("var")) {
            parseVarList();
        } else if (matchKeyword("let") || matchKeyword("const")) {
            parseLetList();
        } else {
            skipUntilTerminal(';');
        }
        skipUntilTerminal(';');
        skipUntilTerminal(';');
        skipUntilTerminal(')');
    }

    void parseStatement() {
        if (eof()) {
            return;
        }
        if (tryPunct(';')) {
            return;
        }
        if (punChar() == '}') {
            return;
        }
        if (punChar() == '{') {
            mustPunct('{');
            enterBlock();
            parseStatementListUntilRBrace();
            tryPunct('}');
            exitBlock();
            return;
        }
        if (matchKeyword("var")) {
            parseVarList();
            tryPunct(';');
            return;
        }
        if (matchKeyword("let") || matchKeyword("const")) {
            parseLetList();
            tryPunct(';');
            return;
        }
        if (peekIdentStr() == "async") {
            size_t save = i;
            ++i;
            if (peekIdentStr() == "function") {
                ++i;
                parseFunctionAfterKeyword(true);
                return;
            }
            i = save;
        }
        if (matchKeyword("function")) {
            parseFunctionAfterKeyword(true);
            return;
        }
        if (matchKeyword("if")) {
            tryPunct('(');
            skipUntilTerminal(')');
            parseStatement();
            if (matchKeyword("else")) {
                parseStatement();
            }
            return;
        }
        if (matchKeyword("while") || matchKeyword("switch")) {
            tryPunct('(');
            skipUntilTerminal(')');
            parseStatement();
            return;
        }
        if (matchKeyword("do")) {
            parseStatement();
            matchKeyword("while");
            tryPunct('(');
            skipUntilTerminal(')');
            tryPunct(';');
            return;
        }
        if (matchKeyword("for")) {
            parseForHeader();
            parseStatement();
            return;
        }
        if (matchKeyword("try")) {
            parseBlock();
            if (matchKeyword("catch")) {
                tryPunct('(');
                enterBlock();
                if ((*tok)[i].kind == Tk::Ident) {
                    std::string c = tokTxt(*code, (*tok)[i]);
                    declareLet(c, (*tok)[i].a, (*tok)[i].b);
                    ++i;
                    tryPunct(')');
                } else {
                    skipUntilTerminal(')');
                }
                mustPunct('{');
                parseStatementListUntilRBrace();
                tryPunct('}');
                exitBlock();
            }
            if (matchKeyword("finally")) {
                parseBlock();
            }
            return;
        }
        if (matchKeyword("return") || matchKeyword("throw")) {
            skipUntilTerminal(';');
            return;
        }
        if (matchKeyword("break") || matchKeyword("continue")) {
            skipUntilTerminal(';');
            return;
        }
        if (matchKeyword("class")) {
            if ((*tok)[i].kind == Tk::Ident) {
                std::string cname = tokTxt(*code, (*tok)[i]);
                declareHoisted(cname, (*tok)[i].a, (*tok)[i].b);
                ++i;
            }
            if (matchKeyword("extends")) {
                skipUntilTerminal('{');
            }
            if (tryPunct('{')) {
                skipBalanced('{', '}');
            }
            return;
        }
        skipUntilTerminal(';');
    }

    void parseProgram() {
        curEnv = &rootEnv;
        rootEnv.parentFn = nullptr;
        rootEnv.hoisted.clear();
        rootEnv.blockStack.clear();
        rootEnv.blockStack.push_back({});
        while (!eof()) {
            if (punChar() == '}') {
                ++i;
                continue;
            }
            parseStatement();
        }
    }
};

static void collectDirectives(const std::string& code, std::unordered_set<std::string>& into) {
    static const char* tag = "@obfuscate:preserve";
    size_t pos = 0;
    while (pos < code.size()) {
        size_t line = code.find("//", pos);
        if (line == std::string::npos) {
            break;
        }
        size_t end = code.find('\n', line);
        if (end == std::string::npos) {
            end = code.size();
        }
        std::string frag = code.substr(line, end - line);
        size_t t = frag.find(tag);
        if (t != std::string::npos) {
            size_t start = t + std::strlen(tag);
            size_t cur = start;
            while (cur < frag.size()) {
                while (cur < frag.size()
                       && (std::isspace(static_cast<unsigned char>(frag[cur])) || frag[cur] == ',')) {
                    ++cur;
                }
                if (cur >= frag.size()) {
                    break;
                }
                size_t w0 = cur;
                while (cur < frag.size() && isIdentPart(frag[cur])) {
                    ++cur;
                }
                if (cur > w0) {
                    into.insert(frag.substr(w0, cur - w0));
                }
            }
        }
        pos = end + 1;
    }
    pos = 0;
    while (pos < code.size()) {
        size_t b = code.find("/*", pos);
        if (b == std::string::npos) {
            break;
        }
        size_t e = code.find("*/", b + 2);
        if (e == std::string::npos) {
            break;
        }
        std::string frag = code.substr(b, e - b);
        size_t t = frag.find(tag);
        if (t != std::string::npos) {
            size_t start = t + std::strlen(tag);
            size_t cur = start;
            while (cur < frag.size()) {
                while (cur < frag.size()
                       && (std::isspace(static_cast<unsigned char>(frag[cur])) || frag[cur] == '*'
                           || frag[cur] == ',')) {
                    ++cur;
                }
                if (cur >= frag.size()) {
                    break;
                }
                size_t w0 = cur;
                while (cur < frag.size() && isIdentPart(frag[cur])) {
                    ++cur;
                }
                if (cur > w0) {
                    into.insert(frag.substr(w0, cur - w0));
                }
            }
        }
        pos = e + 2;
    }
}

static bool isSimpleQuotedIdentifierKey(const std::string& code, const Tok& strTok, std::string& outKey) {
    if (strTok.kind != Tk::Str || strTok.b <= strTok.a + 2) {
        return false;
    }
    if (strTok.b > code.size() || strTok.a >= code.size()) {
        return false;
    }
    char q = code[strTok.a];
    if (q != '\'' && q != '"') {
        return false;
    }
    if (code[strTok.b - 1] != q) {
        return false;
    }
    std::string inner = code.substr(strTok.a + 1, strTok.b - strTok.a - 2);
    if (inner.empty()) {
        return false;
    }
    for (char ch : inner) {
        if (ch == '\\') {
            return false;
        }
    }
    if (!isIdentStart(inner[0])) {
        return false;
    }
    for (size_t i = 1; i < inner.size(); ++i) {
        if (!isIdentPart(inner[i])) {
            return false;
        }
    }
    outKey = std::move(inner);
    return true;
}

static void collectBracketStringKeys(const std::string& code, const std::vector<Tok>& toks,
                                     const std::unordered_set<std::string>* reserved,
                                     std::unordered_set<std::string>& preserve) {
    for (size_t k = 0; k + 2 < toks.size(); ++k) {
        const Tok& t0 = toks[k];
        if (t0.kind != Tk::Pun || t0.a >= code.size() || code[t0.a] != '[') {
            continue;
        }
        std::string key;
        if (!isSimpleQuotedIdentifierKey(code, toks[k + 1], key)) {
            continue;
        }
        const Tok& t2 = toks[k + 2];
        if (t2.kind != Tk::Pun || t2.a >= code.size() || code[t2.a] != ']') {
            continue;
        }
        if (reserved && reserved->find(key) != reserved->end()) {
            continue;
        }
        preserve.insert(key);
    }
}

static void legacySpellingMap(const std::string& code, const ScopeRenameOptions& opt,
                              std::vector<RenameSpan>& spans) {
    std::unordered_map<std::string, std::string> map;
    std::vector<Tok> toks;
    lexAll(code, toks);
    for (const Tok& t : toks) {
        if (t.kind != Tk::Ident) {
            continue;
        }
        std::string id = code.substr(t.a, t.b - t.a);
        if (id.empty() || id[0] == '_') {
            continue;
        }
        if (opt.reserved && opt.reserved->find(id) != opt.reserved->end()) {
            continue;
        }
        if (opt.preserve.find(id) != opt.preserve.end() || opt.externNames.find(id) != opt.externNames.end()) {
            continue;
        }
        if (map.find(id) == map.end()) {
            map[id] = opt.generateMangledName ? opt.generateMangledName() : id;
        }
    }
    for (const Tok& t : toks) {
        if (t.kind != Tk::Ident) {
            continue;
        }
        std::string id = code.substr(t.a, t.b - t.a);
        if (id.empty() || id[0] == '_') {
            continue;
        }
        auto it = map.find(id);
        if (it != map.end()) {
            spans.push_back({t.a, t.b, it->second});
        }
    }
}

}  // namespace

ScopeRenamePlan computeScopedRenames(const std::string& code, const ScopeRenameOptions& optIn,
                                     std::vector<std::string>* topLevelPreservedNamesOut) {
    ScopeRenameOptions opt = optIn;
    collectDirectives(code, opt.preserve);

    ScopeRenamePlan plan;
    if (!opt.reserved || !opt.generateMangledName) {
        plan.usedLegacyFallback = true;
        if (opt.autoPreserveBracketStringKeys) {
            std::vector<Tok> keyToks;
            lexAll(code, keyToks);
            collectBracketStringKeys(code, keyToks, opt.reserved, opt.preserve);
        }
        legacySpellingMap(code, opt, plan.spans);
        return plan;
    }

    std::vector<Tok> toks;
    lexAll(code, toks);
    if (opt.autoPreserveBracketStringKeys) {
        collectBracketStringKeys(code, toks, opt.reserved, opt.preserve);
    }
    Parser p;
    p.opt = opt;
    p.code = &code;
    p.tok = &toks;
    p.i = 0;
    p.topLevelPreservedOut = topLevelPreservedNamesOut;
    p.parseProgram();

    std::sort(p.spans.begin(), p.spans.end(), [](const RenameSpan& a, const RenameSpan& b) {
        if (a.start != b.start) {
            return a.start < b.start;
        }
        return a.end < b.end;
    });
    std::vector<RenameSpan> merged;
    for (const auto& s : p.spans) {
        if (!merged.empty() && s.start == merged.back().start && s.end == merged.back().end) {
            continue;
        }
        merged.push_back(s);
    }
    plan.spans = std::move(merged);

    std::sort(p.undefinedSyms.begin(), p.undefinedSyms.end());
    p.undefinedSyms.erase(std::unique(p.undefinedSyms.begin(), p.undefinedSyms.end()),
                          p.undefinedSyms.end());
    plan.undefinedSymbols = std::move(p.undefinedSyms);
    return plan;
}

}  // namespace geruest::js_scope
