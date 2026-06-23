# Gated Routes & Pages

Custom `bool(const HTTPRequest&)` access checks for static HTML pages and API routes. Unlike [Basic Authentication](BASIC_AUTH.md) (username/password + `401`), gates run your own logic and deny when the handler returns `false`.

| | Pages (`addGatedPage`) | API routes (`addRoute` + gate) | WebSockets (`addRouteWebSocket` + gate) |
|---|------------------------|--------------------------------|----------------------------------------|
| Target | Static HTML from `addRoot()` | Sync or async route handler | WebSocket handler (coroutine or callback) |
| Gate | Sync or async (`PageGateHandler` / `AsyncPageGateHandler` overload) | Optional third argument: sync or async gate | Optional third argument: sync or async gate |
| On deny | **302 Found** redirect | **403 Forbidden** | **403 Forbidden** (before handshake) |
| Registers handler | No (page already on disk) | Yes | Yes |

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

// Async route + async gate (required for DB/session lookup in gate)
server->addRoute("/v1/profile", handleProfileAsync, checkSessionAsync);

// WebSocket with gate: deny → 403 Forbidden (before upgrade)
server->addRouteWebSocket("/chat", chatHandler, [](const HTTPRequest& req) {
    return req.getHeader("authorization") == "Bearer mytoken";
});

server->addRouteWebSocket("/chat", chatHandler, checkSessionAsync);
```

## Async routes require async gates

Async route handlers run on **io_context worker threads** (connection coroutines). There is **no** `addRoute(AsyncRouteHandler, RouteGateHandler)` overload — sync gates on async routes were removed because blocking the worker thread (DB, mutex, `future::get()`) can stall the whole server under load.

Use **`AsyncRouteGateHandler`** and `co_await` for database or session checks:

```cpp
server->addRoute("/v1/profile", handleProfileAsync, [](const HTTPRequest& req) -> AsyncRouteGateAccess {
    auto db = req.database();
    if (!db) {
        co_return false;
    }
    auto rows = co_await db->queryAsync("SELECT 1 FROM sessions WHERE token = $1",
                                        {req.getHeader("authorization")});
    co_return !rows.rows.empty();
});
```

Sync routes may still use sync gates for trivial header checks. Offloading sync gate work to a dedicated pool does **not** apply to app code that blocks inside the route handler itself — use async handlers and `co_await` for I/O there too.

The same gate types apply to WebSocket routes (`addRouteWebSocket(..., gate)`). The gate runs on the upgrade request before `101 Switching Protocols`.

Wrapping a DB call in an app-side `thread_pool` and calling `.get()` from an async route or gate **still blocks** the connection coroutine; use `co_await` on framework async APIs instead.

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
void addRoute(const std::string& path, AsyncRouteHandler handler, AsyncRouteGateHandler gate);
bool removeGatedRoute(const std::string& path);
void clearGatedRoutes();
```

Omit the third argument for an ungated route. Handler and gate types are resolved via C++ overloads (sync or async).

`RouteGateHandler` is `std::function<bool(const HTTPRequest&)>`.

`AsyncRouteGateHandler` is `std::function<boost::asio::awaitable<bool>(const HTTPRequest&)>`.

- Return `true` → route handler runs
- Return `false` → **403 Forbidden** (handler is not called)

Sync gates are for fast, non-blocking checks (headers, tokens in memory). Use `AsyncRouteGateHandler` for database or session lookups. Sync gates run on a dedicated gate thread pool so they do not block io_context workers, but they still cannot `co_await` — use the async gate overload for that.

**Worker threads:** `setWorkerThreadCount` / `WORKER_THREADS` is the number of io_context threads handling connections. Prefer async gates (and async route handlers) for I/O instead of raising the worker count to absorb blocking work.

### WebSocket routes with optional gate

```cpp
void addRouteWebSocket(const std::string& path, WebSocketHandler handler);
void addRouteWebSocket(const std::string& path, WebSocketRoute route);
void addRouteWebSocket(const std::string& path, WebSocketHandler handler, RouteGateHandler gate);
void addRouteWebSocket(const std::string& path, WebSocketHandler handler, AsyncRouteGateHandler gate);
void addRouteWebSocket(const std::string& path, WebSocketRoute route, RouteGateHandler gate);
void addRouteWebSocket(const std::string& path, WebSocketRoute route, AsyncRouteGateHandler gate);
```

Omit the third argument for an ungated WebSocket route. Gate types and path matching are the same as API route gates. See [WebSockets](WEBSOCKETS.md).

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
- Sync route gates run on a gate thread pool (not the connection coroutine); keep them fast or use `AsyncRouteGateHandler` for DB/session work
- Async routes accept only `AsyncRouteGateHandler` (no sync gate overload)

**WebSockets**

- Gate runs before the WebSocket handshake on every matching upgrade request
- Handler exceptions in the gate are treated as denial (`403`)
- Uses the same route gate registry as API routes (`removeGatedRoute` removes the gate; the WebSocket handler stays registered)

## See Also

- [Basic Authentication](BASIC_AUTH.md) — HTTP Basic Auth with `addProtectedPage`
- [Features Overview](FEATURES.md) — full feature list
