# Gated Routes & Pages

Custom `bool(const HTTPRequest&)` access checks for static HTML pages and sync API routes. Unlike [Basic Authentication](BASIC_AUTH.md) (username/password + `401`), gates run your own logic and deny when the handler returns `false`.

| | Pages (`addGatedPage`) | API routes (`addGatedRoute`) |
|---|------------------------|------------------------------|
| Target | Static HTML from `addRoot()` | Sync handlers from `addRoute` |
| On deny | **302 Found** redirect | **403 Forbidden** |
| Registers handler | No (page already on disk) | Yes (`addRoute` + gate in one call) |

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

// API route: deny → 403 Forbidden
server->addGatedRoute("/v1/admin", handleAdmin, [](const HTTPRequest& req) {
    return req.getHeader("authorization") == "Bearer mytoken";
});
```

## API

### Page gates

```cpp
void addGatedPage(const std::string& path, PageGateHandler gate,
                  const std::string& redirectTo = "");
bool removeGatedPage(const std::string& path);
void clearGatedPages();
```

`PageGateHandler` is `std::function<bool(const HTTPRequest&)>`:

- Return `true` → page is served normally
- Return `false` → **302 Found** with `Location` header

#### Redirect behavior

| `redirectTo` | On denial |
|--------------|-----------|
| Empty (default) | Language-aware index: `/de/foo` → `/de/`, otherwise `/` |
| Custom path | Language-aware: `/de/admin` + `/login` → `/de/login` |

Targets that already include a language prefix (e.g. `/en/login`) or an external URL are left unchanged.

### Route gates

```cpp
void addGatedRoute(const std::string& path, RouteHandler handler, RouteGateHandler gate);
bool removeGatedRoute(const std::string& path);
void clearGatedRoutes();
```

`RouteGateHandler` is `std::function<bool(const HTTPRequest&)>`:

- Return `true` → route handler runs
- Return `false` → **403 Forbidden** (handler is not called)

## Path matching

Same rules as routes and redirects:

- Exact paths: `/admin` matches only `/admin`
- Wildcards: `/devices/*` matches `/devices/foo`, `/devices/a/b`

## Interaction with Basic Auth

Both can apply to the same static page. Basic Auth runs first (`401` on failure), then the page gate (`302` on failure).

## Notes

**Pages**

- Gate runs after Basic Auth when serving HTML
- Handler exceptions are treated as denial (redirect)
- Gated pages skip the text-response cache so access is checked on every request
- For database/session checks that need `co_await`, use `addAsyncRoute` to serve the page instead

**API routes**

- Gate runs before the route handler on every matching request
- Handler exceptions are treated as denial (`403`)
- `removeGatedRoute` removes only the gate; the route handler stays registered
- For async routes, use `addRouteAsync` and check access inside the handler

## See Also

- [Basic Authentication](BASIC_AUTH.md) — HTTP Basic Auth with `addProtectedPage`
- [Features Overview](FEATURES.md) — full feature list
