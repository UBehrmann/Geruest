# Security

Built-in utilities that protect every handler in the framework without requiring per-route reimplementation.

## Quick Start

```cpp
#include "security/Security.hpp"

server.addRoute("/api/user", [](const HTTPRequest& req) {
    // Safe SQL — no raw string concatenation needed
    std::string q = Security::buildQuery(
        "SELECT * FROM users WHERE name = ? AND role = ?",
        { req.getParam("name"), req.getParam("role") }
    );

    // Safe JSON — JSONParser escapes automatically
    JSONParser json;
    json.setString("name", req.getParam("name"));
    return responseOK(json.toString());
});
```

## API

```cpp
// SQL
std::string Security::buildQuery(const std::string& queryTemplate,
                                 const std::vector<std::string>& params);
std::string Security::escapeSql(const std::string& input);

// Output encoding
std::string Security::escapeJson(const std::string& input);
std::string Security::escapeHtml(const std::string& input);

// Filesystem
bool        Security::isSafePath(const std::string& root, const std::string& requestPath);
std::string Security::safeCombinePath(const std::string& root, const std::string& requestPath);
```

---

## Example: SQL queries with request parameters

### The problem — direct string concatenation

```cpp
// ❌ UNSAFE — any value from the request is injected verbatim
std::string q = "SELECT * FROM users WHERE name = '" + req.getParam("name") + "'";
// Payload  name = ' OR '1'='1
// Result:  SELECT * FROM users WHERE name = '' OR '1'='1'  ← returns all rows
```

### The solution — `buildQuery`

Replace every user-supplied value with a `?` placeholder and pass the values as a list.  
Each `?` is automatically wrapped in single-quotes and passed through `escapeSql`.

```cpp
// ✅ SAFE — one param
std::string q = Security::buildQuery(
    "SELECT * FROM users WHERE name = ?",
    { req.getParam("name") }
);

// ✅ SAFE — two params
std::string q = Security::buildQuery(
    "SELECT * FROM users WHERE name = ? AND role = ?",
    { req.getParam("name"), req.getParam("role") }
);

// ✅ SAFE — INSERT
std::string q = Security::buildQuery(
    "INSERT INTO messages (author, body) VALUES (?, ?)",
    { req.getParam("author"), req.getParam("body") }
);
```

If the number of `?` placeholders does not match the number of values supplied,  
`buildQuery` throws `std::invalid_argument` immediately — a mismatch is caught at  
runtime rather than silently producing a broken query.

### What `escapeSql` handles

| Character | Replacement | Reason |
|-----------|-------------|--------|
| `'` | `''` | ANSI SQL delimiter — the primary injection vector |
| `"` | `\"` | Alternative delimiter (SQLite, older PostgreSQL) |
| `\` | `\\` | MySQL/MariaDB escape character — prevents `\'` bypass |
| NUL `\x00` | `\0` | Truncates C-string queries in older DB drivers |
| SUB `\x1a` | `\Z` | MySQL/Windows EOF byte, silently cuts off queries |

`;`, `--`, `#`, and `/*` are **not** modified because they are only SQL syntax  
*outside* a string literal.  The `'` doubling prevents the attacker from ever  
leaving the string context, making these characters inert data.

### When a prepared-statement API is available — always prefer it

`buildQuery` / `escapeSql` are fallback helpers for situations where your DB  
library does not provide a binding API.  When it does, use it:

```cpp
// libpq (PostgreSQL)
const char* params[] = { req.getParam("name").c_str() };
PGresult* res = PQexecParams(conn,
    "SELECT * FROM users WHERE name = $1",
    1, nullptr, params, nullptr, nullptr, 0);

// SQLite
sqlite3_stmt* stmt;
sqlite3_prepare_v2(db, "SELECT * FROM users WHERE name = ?", -1, &stmt, nullptr);
sqlite3_bind_text(stmt, 1, req.getParam("name").c_str(), -1, SQLITE_TRANSIENT);
```

---

## Example: JSON responses

`JSONParser::setString` calls `Security::escapeJson` internally — you get safe  
output automatically.

```cpp
// ✅ SAFE — automatic escaping via JSONParser
JSONParser json;
json.setString("name",    req.getParam("name"));    // e.g. say "hello"  →  "say \"hello\""
json.setString("message", req.getParam("message")); // newlines, backslashes handled
response.setBody(json.toString());
```

If you build a JSON string manually, call `escapeJson` yourself:

```cpp
// ✅ SAFE — manual escape
std::string safe = Security::escapeJson(req.getParam("name"));
response.setBody(R"({"name":")" + safe + R"("})");

// ❌ UNSAFE — raw value breaks JSON when input contains " or \
response.setBody(R"({"name":")" + req.getParam("name") + R"("})");
```

### What `escapeJson` handles

All RFC 8259 requirements: `"` `\` and the full C0 control range (U+0000–U+001F),  
including `\n` `\r` `\t` `\b` `\f` and `\uXXXX` for remaining control characters.

---

## Example: HTML responses

```cpp
// ✅ SAFE
std::string name = Security::escapeHtml(req.getParam("name"));
response.setBody("<p>Hello, " + name + "</p>");

// ❌ UNSAFE — <script> tag in name becomes live JavaScript
response.setBody("<p>Hello, " + req.getParam("name") + "</p>");
```

| Character | Replacement |
|-----------|-------------|
| `&`  | `&amp;`  |
| `<`  | `&lt;`   |
| `>`  | `&gt;`   |
| `"`  | `&quot;` |
| `'`  | `&#x27;` |

---

## Example: Filesystem paths

`buildPath` calls `Security::isSafePath` automatically, so static file serving  
is protected out of the box.  Use the helpers explicitly in route handlers that  
construct filesystem paths from request data:

```cpp
server.addRoute("/download/*", [&](const HTTPRequest& req) {
    std::string filePath = Security::safeCombinePath(
        server.getRoot() + "/files",
        req.getPathString().substr(9)   // strip "/download"
    );

    if (filePath.empty()) {
        return responseForbidden(); // .. traversal blocked
    }

    // open and serve filePath ...
    return responseOK();
});
```

`safeCombinePath` returns an empty string whenever the resolved path would escape  
the root directory (e.g. `/../etc/passwd`, `/html/../../etc/shadow`).

**⚠️ The framework protects its own static-file serving automatically.  You only  
need to call `isSafePath` / `safeCombinePath` in route handlers that build  
filesystem paths themselves.**
