# Page Gates

Custom access checks for static HTML pages. Unlike [Basic Authentication](BASIC_AUTH.md) (username/password + `401`), page gates run your own logic and redirect on denial.

## Quick Start

```cpp
#include <Geruest.hpp>
using namespace geruest;

auto server = std::make_unique<Geruest>();

server->addGatedPage("/devices/devices", [](const HTTPRequest& req) {
    return req.getParam("token") == "secret"
        || req.getHeader("authorization") == "Bearer mytoken";
});

server->addGatedPage("/admin/dashboard", checkAdminSession, "/login");
```

## API

```cpp
void addGatedPage(const std::string& path, PageGateHandler gate,
                  const std::string& redirectTo = "");
bool removeGatedPage(const std::string& path);
void clearGatedPages();
```

`PageGateHandler` is `std::function<bool(const HTTPRequest&)>`:

- Return `true` → page is served normally
- Return `false` → client receives **302 Found** with `Location` header

## Redirect behavior

| `redirectTo` | On denial |
|--------------|-----------|
| Empty (default) | Language-aware index: `/de/foo` → `/de/`, otherwise `/` |
| Custom path | Exact value you provide (e.g. `/login`) |

## Path matching

Same rules as routes and redirects:

- Exact paths: `/admin` matches only `/admin`
- Wildcards: `/devices/*` matches `/devices/foo`, `/devices/a/b`

## Interaction with Basic Auth

Both can apply to the same page. Basic Auth runs first (`401` on failure), then the page gate (`302` on failure).

## Notes

- Only applies to **static HTML** pages served from `addRoot()` (not API routes from `addRoute`)
- Handler exceptions are treated as denial (redirect)
- Gated pages skip the text-response cache so access is checked on every request
- For database/session checks that need `co_await`, use `addRoute` to serve the page instead

## See Also

- [Basic Authentication](BASIC_AUTH.md) — HTTP Basic Auth with `addProtectedPage`
- [Features Overview](FEATURES.md) — full feature list
