/**
 * @file Security.hpp
 * @date 27.03.2026
 *
 * @author Urs Behrmann
 *
 * @brief Framework-level security utilities: output encoding, input validation,
 *        and path-traversal protection. Call these helpers so individual route
 *        handlers do not have to reimplement sanitisation themselves.
 *
 * ## SQL injection
 * The framework carries no database driver, so the correct defence is always
 * to use the parameterised-query API of your chosen DB library (libpq, SQLite,
 * etc.).  `escapeSql()` is provided only as a last-resort fallback for legacy
 * situations where a prepared-statement API is not available.
 *
 * ## Output encoding
 * Use `escapeJson()` whenever you embed a runtime string inside a JSON
 * literal.  Prefer `JSONParser` (which calls `escapeJson()` internally) over
 * hand-built JSON strings.
 *
 * Use `escapeHtml()` when embedding user-supplied text in HTML markup.
 */

#ifndef GERUEST_SECURITY_HPP
#define GERUEST_SECURITY_HPP

#include <stdexcept>
#include <string>
#include <vector>

namespace geruest {

class Security {
   public:
    /**
     * Escape a string for safe inclusion as a JSON string *value*.
     *
     * Encodes the following characters per RFC 8259:
     *   `\`  `"`  and the C0 control characters (U+0000–U+001F).
     *
     * Example:
     * @code
     *   // Without escaping this would produce broken JSON:
     *   // {"name":"O'Brien"}  — OK (single-quote is fine)
     *   // {"name":"say "hi""}  — BROKEN
     *   json.setString("name", req.getParam("name"));   // JSONParser calls escapeJson internally
     *   // Or manually:
     *   std::string safe = Security::escapeJson(userInput);
     * @endcode
     */
    static std::string escapeJson(const std::string& input);

    /**
     * Encode a string for safe embedding in HTML content or attribute values.
     *
     * Replaces: `&`  `<`  `>`  `"`  `'`
     *
     * Example:
     * @code
     *   std::string html = "<p>" + Security::escapeHtml(req.getParam("msg")) + "</p>";
     * @endcode
     */
    static std::string escapeHtml(const std::string& input);

    /**
     * Escape a string for inclusion in a SQL string literal.
     *
     * @warning **This is a last-resort fallback.** Parameterised queries /
     *          prepared statements are always the correct and preferred
     *          defence against SQL injection.  Only use this helper when your
     *          DB library provides no other option.
     *
     * Escapes the following characters:
     *
     * | Character | Replacement | Reason |
     * |-----------|-------------|--------|
     * | `'`  | `''`   | ANSI SQL string delimiter — primary injection vector |
     * | `"`  | `\"`   | Alternative string delimiter (SQLite, older PostgreSQL) |
     * | `\`  | `\\`   | MySQL/MariaDB escape character — prevents `\'` bypass |
     * | NUL  | `\0`   | Truncates C-string queries in older drivers |
     * | SUB (\\x1a) | `\Z` | MySQL/Windows EOF byte |
     *
     * **Why `;`, `--`, `#`, and `/ *` are not escaped:**
     * These are SQL syntax only *outside* a string literal.  Once `'` is
     * doubled the attacker can never leave the string context, so semicolons
     * and comment markers become harmless character data.
     *
     * You must still wrap the result in single-quotes in the query.
     *
     * Example:
     * @code
     *   // Correct: prepared statement (preferred)
     *   stmt.bind(1, req.getParam("name"));
     *
     *   // Fallback only: manual escaping
     *   std::string safe = Security::escapeSql(req.getParam("name"));
     *   std::string query = "SELECT * FROM users WHERE name = '" + safe + "'";
     * @endcode
     */
    static std::string escapeSql(const std::string& input);

    /**
     * Verify that a resolved filesystem path stays inside a trusted root.
     *
     * Returns `true` only when the canonical form of `root + requestPath`
     * begins with the canonical root directory, blocking `../` traversal
     * attacks.
     *
     * @param root        Absolute filesystem root (e.g. serverData.getRoot()).
     * @param requestPath URL-derived path segment (may contain `..`).
     *
     * @code
     *   if (!Security::isSafePath(serverData.getRoot(), req.getPathString())) {
     *       return responseForbidden();
     *   }
     * @endcode
     */
    static bool isSafePath(const std::string& root, const std::string& requestPath);

    /**
     * Combine `root` and `requestPath` only when the result stays inside
     * `root`; returns an empty string on traversal attempts.
     *
     * This is a convenience wrapper around `isSafePath()`.
     */
    static std::string safeCombinePath(const std::string& root, const std::string& requestPath);

    /**
     * Build a SQL query string by substituting `?` placeholders with
     * properly escaped, single-quoted values — one per entry in `params`.
     *
     * This is the recommended way to combine multiple user-supplied values
     * into a query when a prepared-statement API is not available.  Each
     * placeholder is handled automatically, so it is impossible to forget
     * to escape one of the arguments.
     *
     * Throws `std::invalid_argument` if the number of `?` in `queryTemplate`
     * does not match the size of `params`.
     *
     * Example:
     * @code
     *   std::string q = Security::buildQuery(
     *       "SELECT * FROM users WHERE name = ? AND email = ?",
     *       { req.getParam("name"), req.getParam("email") }
     *   );
     * @endcode
     *
     * Produces:
     * @code
     *   SELECT * FROM users WHERE name = 'alice' AND email = 'alice@example.com'
     * @endcode
     *
     * @param queryTemplate  SQL template with `?` as positional placeholders.
     * @param params         Values to substitute, in order.
     */
    static std::string buildQuery(const std::string& queryTemplate,
                                  const std::vector<std::string>& params);
};

}  // namespace geruest

#endif  // GERUEST_SECURITY_HPP
