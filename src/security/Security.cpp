/**
 * @file Security.cpp
 * @date 27.03.2026
 *
 * @author Urs Behrmann
 */

#include "Security.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace geruest {

namespace {

/**
 * Build the same logical path as string concatenation `root + requestPath` would
 * imply for URL-style segments that begin with `/`, without letting an absolute
 * `requestPath` replace `root` when using `std::filesystem::path::operator/`.
 */
std::filesystem::path combinedPathForCanonicalCheck(const std::string& root, const std::string& requestPath) {
    namespace fs = std::filesystem;
    fs::path r(root);
    if (requestPath.empty()) {
        return r;
    }
    fs::path p(requestPath);
    if (p.is_absolute()) {
        return r / p.relative_path();
    }
    return r / p;
}

}  // namespace

// ─── JSON escaping ──────────────────────────────────────────────────────────

std::string Security::escapeJson(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (unsigned char c : input) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Encode remaining C0 control characters as \uXXXX
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += static_cast<char>(c);
                }
                break;
        }
    }

    return result;
}

// ─── HTML escaping ──────────────────────────────────────────────────────────

std::string Security::escapeHtml(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (unsigned char c : input) {
        switch (c) {
            case '&':  result += "&amp;";   break;
            case '<':  result += "&lt;";    break;
            case '>':  result += "&gt;";    break;
            case '"':  result += "&quot;";  break;
            case '\'': result += "&#x27;";  break;
            default:   result += static_cast<char>(c); break;
        }
    }

    return result;
}

// ─── SQL escaping ───────────────────────────────────────────────────────────

std::string Security::escapeSql(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (unsigned char c : input) {
        switch (c) {
            // ── String delimiters ─────────────────────────────────────────────
            case '\'':
                // ANSI SQL: double the single-quote.  This is the primary
                // defence — it keeps the value inside its string literal and
                // makes ; -- # /* harmless (they are only syntax OUTSIDE quotes).
                result += "''";
                break;
            case '"':
                // Some dialects (SQLite, older PostgreSQL) allow "string"
                // literals.  Escape the double-quote so it cannot close such
                // a delimiter.
                result += "\\\"";
                break;

            // ── Backslash ─────────────────────────────────────────────────────
            case '\\':
                // MySQL / MariaDB treat backslash as an escape character inside
                // strings (e.g. \' would reopen the injection).  Double it.
                result += "\\\\";
                break;

            // ── Dangerous bytes ───────────────────────────────────────────────
            case '\0':
                // NUL terminates C-strings and can truncate queries in older
                // DB drivers.
                result += "\\0";
                break;
            case '\x1a':
                // Ctrl-Z (SUB) — MySQL on Windows interprets it as EOF, which
                // can silently cut off the remainder of a query.
                result += "\\Z";
                break;

            default:
                result += static_cast<char>(c);
                break;
        }
    }

    // Note: ; -- # /* are NOT escaped because they are only SQL syntax
    // OUTSIDE a string literal.  The ' escaping above prevents an attacker
    // from ever leaving the string context in the first place.
    return result;
}

// ─── Parameterised query builder ────────────────────────────────────────────

std::string Security::buildQuery(const std::string& queryTemplate,
                                 const std::vector<std::string>& params) {
    // Count placeholders so we can validate before substituting anything.
    size_t placeholderCount = 0;
    for (char c : queryTemplate) {
        if (c == '?') ++placeholderCount;
    }

    if (placeholderCount != params.size()) {
        throw std::invalid_argument(
            "Security::buildQuery: query has " + std::to_string(placeholderCount) +
            " placeholder(s) but " + std::to_string(params.size()) + " value(s) were supplied");
    }

    std::string result;
    result.reserve(queryTemplate.size() + params.size() * 16);

    size_t paramIndex = 0;
    for (char c : queryTemplate) {
        if (c == '?') {
            result += '\'';
            result += escapeSql(params[paramIndex++]);
            result += '\'';
        } else {
            result += c;
        }
    }

    return result;
}

// ─── Path traversal protection ──────────────────────────────────────────────

bool Security::isSafePath(const std::string& root, const std::string& requestPath) {
    if (root.empty()) {
        return false;
    }
    namespace fs = std::filesystem;
    const fs::path combined = combinedPathForCanonicalCheck(root, requestPath);
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(combined, ec);
    if (ec) {
        return false;
    }
    const fs::path canonicalRoot = fs::weakly_canonical(fs::path(root), ec);
    if (ec) {
        return false;
    }
    const auto [rootEnd, _] = std::mismatch(canonicalRoot.begin(), canonicalRoot.end(),
                                           canonical.begin(), canonical.end());
    return rootEnd == canonicalRoot.end();
}

std::string Security::safeCombinePath(const std::string& root, const std::string& requestPath) {
    if (!isSafePath(root, requestPath)) {
        return "";
    }
    return root + requestPath;
}

}  // namespace geruest
