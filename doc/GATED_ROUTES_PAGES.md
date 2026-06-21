# Gated Routes & Pages

Custom `bool(const HTTPRequest&)` access checks for static HTML pages and API routes. Unlike [Basic Authentication](BASIC_AUTH.md) (username/password + `401`), gates run your own logic and deny when the handler returns `false`.

| | Pages (`addGatedPage`) | API routes (`addRoute` + gate) |
|---|------------------------|--------------------------------|
| Target | Static HTML from `addRoot()` | Sync or async route handler |
| Gate | Sync or async (`PageGateHandler` / `AsyncPageGateHandler` overload) | Optional third argument: sync or async gate |
| On deny | **302 Found** redirect | **403 Forbidden** |
| Registers handler | No (page already on disk) | Yes |

## Quick Start

```cpp
#include <Geruest.hpp>
using namespace geruest;

auto server = std::make_unique<Geruest>();

// Static page: deny → 302 redirect
server->addGatedPage("/devices/devices", [](const HTTPRequest& req) {
    return req.getParam("token") == "secret"
        || req.getHeader("authorization") == "Bearer mytoken";
});

server->addGatedPage("/admin/dashboard", checkAdminSession, "/login");

// Async page gate: deny → 302 redirect (use when gate needs co_await, e.g. DB session)
server->addGatedPage("/admin/dashboard", checkAdminSessionAsync, "/login");

// API route without gate
server->addRoute("/v1/public", handlePublic);

// API route with gate: deny → 403 Forbidden
server->addRoute("/v1/admin", handleAdmin, [](const HTTPRequest& req) {
    return req.getHeader("authorization") == "Bearer mytoken";
});

// Async route + sync gate
server->addRoute("/v1/profile", handleProfileAsync, checkSession);

// Async route + async gate (DB/session lookup in gate)
server->addRoute("/v1/profile", handleProfileAsync, checkSessionAsync);
```

## API

### Page gates

```cpp
void addGatedPage(const std::string& path, PageGateHandler gate,
                  const std::string& redirectTo = "");
void addGatedPage(const std::string& path, AsyncPageGateHandler gate,
                  const std::string& redirectTo = "");
bool removeGatedPage(const std::string& path);
void clearGatedPages();
```

`PageGateHandler` is `std::function<bool(const HTTPRequest&)>`.

`AsyncPageGateHandler` is `std::function<boost::asio::awaitable<bool>(const HTTPRequest&)>` (alias `AsyncPageGateAccess` in `geruest` namespace).

- Return `true` → page is served normally
- Return `false` → **302 Found** with `Location` header

#### Redirect behavior

| `redirectTo` | On denial |
|--------------|-----------|
| Empty (default) | Language-aware index: `/de/foo` → `/de/`, otherwise `/` |
| Custom path | Language-aware: `/de/admin` + `/login` → `/de/login` |

Targets that already include a language prefix (e.g. `/en/login`) or an external URL are left unchanged.

### API routes with optional gate

```cpp
void addRoute(const std::string& path, RouteHandler handler);
void addRoute(const std::string& path, AsyncRouteHandler handler);
void addRoute(const std::string& path, RouteHandler handler, RouteGateHandler gate);
void addRoute(const std::string& path, RouteHandler handler, AsyncRouteGateHandler gate);
void addRoute(const std::string& path, AsyncRouteHandler handler, RouteGateHandler gate);
void addRoute(const std::string& path, AsyncRouteHandler handler, AsyncRouteGateHandler gate);
bool removeGatedRoute(const std::string& path);
void clearGatedRoutes();
```

Omit the third argument for an ungated route. Handler and gate types are resolved via C++ overloads (sync or async).

`RouteGateHandler` is `std::function<bool(const HTTPRequest&)>`.

`AsyncRouteGateHandler` is `std::function<boost::asio::awaitable<bool>(const HTTPRequest&)>`.

- Return `true` → route handler runs
- Return `false` → **403 Forbidden** (handler is not called)

Sync gates must not block on I/O (no `co_await`). Use `AsyncRouteGateHandler` for database or session lookups.

## Path matching

Same rules as routes and redirects:

- Exact paths: `/admin` matches only `/admin`
- Wildcards: `/devices/*` matches `/devices/foo`, `/devices/a/b`
- **Longest wildcard wins** when several patterns match (for both routes and gates)
- **Canonical paths:** `/devices/devices.html` and `/devices/devices/` match a gate registered on `/devices/devices`
- Language prefix: `/de/admin` also matches gates registered on `/admin` when languages are configured

### Precedence (page gates)

When multiple page gate rules could match, resolution order is:

1. **Exact path** on the request path (after canonicalization)
2. **Longest matching wildcard** on the request path
3. Repeat 1–2 on the path **without** the language prefix (`/de/admin` → `/admin`)

On the **same exact path**, an async page gate wins over a sync gate.

Among wildcards, the **longest pattern** wins; ties break lexicographically (smaller pattern string wins).

Runtime enforcement uses `findResolvedPageGate`; prefer registering gates on extensionless paths.

### Precedence (route handlers)

When sync and async route handlers are registered on the **same path pattern**, the **async handler wins** at dispatch.

### Route + gate alignment

Pass the gate as the third argument to `addRoute` on the **same path pattern** as the handler. Route lookup and gate lookup use the same longest-wildcard and language-prefix rules.

You can also register a route and gate separately (`addRoute` + `addRouteGate`), but the gate path must cover every request that should be protected.

## Interaction with Basic Auth

Both can apply to the same static page. Basic Auth runs first (`401` on failure), then the page gate (`302` on failure).

## Notes

**Pages**

- Gate runs after Basic Auth when serving HTML
- Handler exceptions are treated as denial (redirect)
- Gated pages skip the text-response cache so access is checked on every request
- Use the `AsyncPageGateHandler` overload of `addGatedPage` when the gate needs `co_await` (database/session lookup)
- With `mergeAssets=true`, **merged JS/CSS bundles** for a gated or Basic Auth page are protected automatically (same gate/auth as the HTML). Denied asset requests return **403** (not 302). Requires `mergeAssets=true`; individual shared scripts with merging disabled are not auto-protected.

**API routes**

- Gate runs before the route handler on every matching request (sync and async)
- Handler exceptions are treated as denial (`403`)
- `removeGatedRoute` removes only the gate; the route handler stays registered
- Sync route gates block the connection coroutine — keep them fast or use `AsyncRouteGateHandler`

## See Also

- [Basic Authentication](BASIC_AUTH.md) — HTTP Basic Auth with `addProtectedPage`
- [Features Overview](FEATURES.md) — full feature list
