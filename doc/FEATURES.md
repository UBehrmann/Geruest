# Features Overview

## Quick Reference

| Feature | Description | Key Method/Header |
|---------|-------------|-------------------|
| **Routing** | Sync and async handlers via `addRoute` (overload), WebSocket via `addRouteWebSocket`; exact/wildcard paths | `addRoute()`, `addRouteWebSocket()` |
| **Static Files** | Auto-serve files from root directories | `addRoot()` |
| **Templates** | Component injection `{file}`, translations `[key]` | `ContentBuilder` |
| **Asset Merging** | Combine CSS/JS from HTML `<link>` / `<script>` tags per page | `AssetMerger` |
| **JS Obfuscation** | Make JavaScript harder to analyze | `setObfuscationLevel()` |
| **Translations** | Multi-language with JSON files | `setAvailableLanguages()` |
| **CORS** | Path-scoped allowlist + OPTIONS preflight | `enableCors()` |
| **Basic Auth** | SHA-256 password protection | `BasicAuth` |
| **Gated Pages & Routes** | Custom access checks for HTML pages (302) and API routes (403) | `addGatedPage()`, `addRoute(..., gate)` |
| **Email/SMTP** | Send emails via SMTP (with TLS) | `EmailSender` |
| **JSON Parsing** | String-based JSON handling | `JSONParser` |
| **WebP Conversion** | Auto-convert images to WebP | `WebPConverter` |
| **Configuration** | `.env` and environment config | `ConfigLoader` |
| **Async HTTP I/O** | Boost.Asio, C++20 coroutines in the handler | `Geruest::start()` / internal `HttpSession` |

## Server I/O and thread pool

The HTTP server is built on **Boost.Asio**:

- A shared **`boost::asio::io_context`** drives the event loop.
- **`WORKER_THREADS`** (or `setWorkerThreadCount`) is the number of threads calling `io_context::run()` (default: hardware concurrency × 2). The thread that calls `start()` also runs the `io_context` until `stop()`.
- New connections are accepted with **`async_accept`**. Each client gets an **`HttpSession`** (`shared_ptr`, `co_spawn` on a **per-connection strand**) that runs **`Handler::runAsync()`** — non-blocking reads/writes with **`co_await`** on the TCP socket.
- **`MAX_QUEUE_SIZE`** / `setMaxQueueSize` sets the **maximum number of concurrent client sessions** (in-flight HTTP connections), not a backlog queue of pending file descriptors. When the limit is reached, new TCP accepts are **closed immediately** and counted as **queue rejections** in `/status`.

## Routing

Use `addRoute` for sync or async handlers (C++ overload picks the handler type). Pass an optional third argument for an access gate. Use `addRouteWebSocket` for persistent WebSocket connections. See [WebSockets](WEBSOCKETS.md).

**Precedence:** When sync and async handlers are registered on the **same path pattern**, the **async handler wins** at dispatch. Avoid registering both unless you intend the async handler to replace the sync one.

```cpp
// Exact match (O(1) lookup)
server.addRoute("/api/users", [](const HTTPRequest& req) {
    HTTPResponse res("200 OK");
    res.setBody(R"({"users":[]})");
    return res;
});

// Wildcards (O(n) pattern matching)
geruest::RouteHandler routeHandler = [](const HTTPRequest& req) {
    (void)req;
    return responseOK();
};
geruest::RouteHandler apiHandler = [](const HTTPRequest& req) {
    (void)req;
    return responseOK();
};
server.addRoute("/users/*/profile", routeHandler);  // /users/123/profile
server.addRoute("/api/*", apiHandler);              // /api/anything

// Async route for DB calls
server.addRoute("/api/users/db", [](const HTTPRequest& req) -> geruest::AsyncResponse {
    auto db = req.database();
    if (!db) {
        co_return responseInternalServerError();
    }
    auto rows = co_await db->queryAsync("SELECT id FROM users", {});
    (void)rows;
    co_return responseOK();
});

// Gated API route (optional third argument)
server.addRoute("/v1/admin", handleAdmin, checkSession);
server.addRoute("/v1/profile", handleProfileAsync, checkSessionAsync);
```

## Static File Serving

```cpp
server.addRoot("/var/www/website");  // Auto-serve files
// Serves: /assets/styles.css, /images/logo.png, etc.
```

## Template System

```html
<!-- Component injection -->
{components/header.html}
{components/footer.html}

<!-- Translations -->
<h1>[assets/translations/home.json:title]</h1>
```

## Asset Merging

Asset merge scans each HTML page for `<link rel="stylesheet">` and `<script src="...">` tags, concatenates referenced files into one CSS and one JS bundle per page, and rewrites the HTML. No JSON file maps are required.

```html
<!-- Input -->
<link rel="stylesheet" href="reset.css">
<link rel="stylesheet" href="layout.css">
<script src="utils.js"></script>
<script src="main.js"></script>

<!-- Output (setMergeAssets(true)) -->
<link rel="stylesheet" href="/pagename.css">
<script src="/pagename.js"></script>
```

See [ASSET_MERGING.md](ASSET_MERGING.md) for path normalization and subdirectory behavior.

## JavaScript Obfuscation

Protect your JavaScript code from casual analysis and copying:

```cpp
server.setObfuscationLevel(2);  // 0=off, 1=basic, 2=medium, 3=advanced
server.setObfuscationCacheExpiry(7);  // Days to cache (default: 7)

// Exclude external libraries
server.addObfuscationExclusion("jquery.min.js");
server.addObfuscationExclusion("bootstrap.min.js");
```

**Obfuscation Levels:**
- **Level 0**: Disabled (default)
- **Level 1**: Name mangling + whitespace removal
- **Level 2**: Level 1 + string/number encoding
- **Level 3**: Level 2 + dead code + control flow obfuscation

**Key Features:**
- ✅ Automatic caching with expiry
- ✅ Respects dev mode (auto-disables)
- ✅ Works with asset merging
- ✅ Excluded files not merged or obfuscated

See [OBFUSCATION.md](OBFUSCATION.md) for complete documentation.

## Multi-Language

```cpp
server.setAvailableLanguages({"en", "de", "fr"});  // First = default
```

URLs get language prefixes: `/about` → `/en/about`, `/de/about`, `/fr/about`

## CORS Support

**CORS is off by default.** You only need `enableCors()` when a **browser** loads your frontend from one origin (scheme + host + port) and calls your Geruest API on another — for example a React dev server on `http://localhost:5173` talking to Geruest on `http://localhost:8080`, or a SPA on `https://app.example.com` calling `https://api.example.com`.

You do **not** need CORS when:

- HTML/JS is served by the same Geruest instance and uses **relative URLs** (`fetch("/v1/users")`) — same origin, no CORS involved
- Callers are not browsers: `curl`, mobile native apps, backend services, cron jobs
- You terminate everything behind one hostname (e.g. nginx serves the SPA and proxies `/v1/` to Geruest on the same `example.com`)

### Enable once, before `init()`

```cpp
server.enableCors({
    .origins = {"http://localhost:5173", "https://app.example.com"},
    .paths   = {"/v1/*", "/api/*"},
});
```

| Field | Meaning |
|-------|---------|
| `origins` | Exact allowed `Origin` header values, or `"*"` for any origin (convenient for local dev only) |
| `paths` | URL paths that get CORS headers — exact match or `*` wildcard, same rules as routes |

What Geruest does when enabled:

1. **OPTIONS preflight** — Browser sends `OPTIONS` before non-simple cross-origin requests. Geruest answers with `204 No Content` and the right `Access-Control-*` headers when the path and origin match.
2. **Actual responses** — `GET`, `POST`, etc. on matching paths get `Access-Control-Allow-Origin` (and related headers) added automatically on route responses. You do not set them in each handler.

```cpp
server.enableCors({
    .origins = {"https://app.example.com"},
    .paths   = {"/v1/*"},
});

server.addRoute("/v1/users", [](const HTTPRequest& req) -> boost::asio::awaitable<HTTPResponse> {
    HTTPResponse res("200 OK");
    res.setHeader("Content-Type", "application/json");
    res.setBody(R"({"users":[]})");
    co_return res;  // CORS headers added by the framework
});
```

### Typical setups

**Local SPA + API (different ports):**

```cpp
server.enableCors({
    .origins = {"http://localhost:3000", "http://localhost:5173"},
    .paths   = {"/v1/*", "/api/*"},
});
```

**Production — list real frontend origins, never `"*"` unless you accept any site calling your API:**

```cpp
server.enableCors({
    .origins = {"https://app.example.com", "https://www.example.com"},
    .paths   = {"/v1/*"},
});
```

**Monolith (website + API from Geruest):** skip `enableCors()` — use relative paths in JS.

### Allowed methods and headers

Preflight allows: `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`.

Default allowed request headers: `Content-Type`, `Authorization`. If the browser sends `Access-Control-Request-Headers` (e.g. a custom `X-Api-Key`), that value is echoed back on preflight.

### Security notes

- Disallowed origins receive **no** CORS headers; preflight from a bad origin gets `403 Forbidden`.
- `"*"` allows **any** website's JavaScript to call your API from users' browsers. Use only for local development.
- CORS is a **browser** policy, not authentication. It does not stop direct HTTP calls to your API.
- `enableCors()` applies to **HTTP routes** on matching paths, not WebSocket upgrades (see [WEBSOCKETS.md](WEBSOCKETS.md)).

### Manual headers (legacy)

Per-route CORS headers still work but are redundant when `enableCors()` covers that path. Prefer `enableCors()` for API prefixes.


## Basic Authentication

```cpp
#include <auth/BasicAuth.hpp>

BasicAuth auth;
auth.addUser("admin", BasicAuth::hashPassword("secret123"));

server.addRoute("/admin", [&auth](const HTTPRequest& req) {
    if (!auth.authenticate(req.getPathString(), req.getHeader("Authorization"))) {
        return responseUnauthorizedBasicAuth("Admin Area");
    }
    HTTPResponse response("200 OK");
    response.setBody("Admin Dashboard");
    return response;
});
```

**Note:** This framework does not provide built-in HTTPS/TLS support. For production deployments with authentication, use a reverse proxy (nginx, Apache, Caddy) to handle TLS termination.

## Gated Pages & Routes

Custom access checks for static HTML pages and API routes. Page gates deny with **302**; route gates deny with **403**.

```cpp
server.addGatedPage("/devices/devices", [](const HTTPRequest& req) {
    return req.getParam("token") == "secret";
}, "/login");  // pages: optional redirect; omit for language-aware index

server.addGatedPage("/admin", checkSessionAsync, "/login");  // async gate overload (co_await)

server.addRoute("/v1/secret", handleSecret, checkSession);

server.addRoute("/v1/profile", handleProfileAsync, checkSessionAsync);
```

See [GATED_ROUTES_PAGES.md](GATED_ROUTES_PAGES.md) for full API.

## Email (`EmailSender`)

Requires libcurl at build time (`GERUEST_HAS_CURL`). See [EMAIL.md](EMAIL.md).

```cpp
server.loadConfig(".env");  // or server.initEmail(...)

EmailSender::getInstance().enqueueEmail(
    "to@example.com",
    "Subject",
    "Message body",
    req.getClientIP()  // used for per-IP send limits, not global HTTP rate limiting
);
```

## JSON Processing

```cpp
JSONParser json;
json.parse(R"({"name":"John","age":30})");
std::string name = json.getString("name");
int age = json.getInt("age");

// Building JSON
json.setString("status", "success");
json.setInt("code", 200);
std::string output = json.build();  // {"status":"success","code":200}
```

## WebP Conversion

```cpp
#include <builders/WebPConverter.hpp>

WebPConverter::convertToWebP("input.png", "output.webp", 90);  // Quality: 90%
```

## Configuration System

**Priority:** code setters (`server.setPort()`, etc.) > `.env` > environment variables when using `server.loadConfig()`. Standalone `ConfigLoader::get*()` uses `.env` > environment variables > default argument. See [CONFIGURATION.md](CONFIGURATION.md).

**`.env` file:**
```env
PORT=8080
DEV_MODE=false
SMTP_SERVER=smtp.gmail.com
```

**Reading configuration:**
```cpp
geruest::ConfigLoader::loadEnvFile(".env");
int port = geruest::ConfigLoader::getInt("PORT", 8080);  // Default: 8080
```

## Advanced Patterns

**Graceful Shutdown:**
```cpp
std::unique_ptr<Geruest> server = std::make_unique<Geruest>();

std::signal(SIGINT, [](int) { 
    if (server) server->stop(); 
});

server->setPort(8080);
server->init();
server->start();
```

**Custom Error Pages:**
```cpp
server.set404("/404.html");
```

**Redirects:**
```cpp
server.addRedirect("/old-home", "/en/home");          // 301
server.addRedirect("/temp-news", "/en/news", 302);    // 302
server.addRedirect("/go/*", "/en/*");                 // wildcard forwarding

server.addRedirects({
    {"/gh", "https://github.com/UBehrmann/Geruest"},
    {"/docs", "/en/documentation"}
});
```

**Request Logging:**
```cpp
server.addRoute("/api/*", [](const HTTPRequest& req) {
    sendToLoggerAPI(req.getMethod() + " " + req.getPath());
    // ... handle request ...
    return responseOK();
});
```

## Performance Tips

- **Route Lookup**: Prefer exact routes over wildcards (O(1) vs O(n))
- **Static Files**: Bypass routing entirely for better performance
- **Asset Merging**: Reduces HTTP requests dramatically
- **WebP**: Smaller image sizes, faster loading
- **Threading**: Tune `setWorkerThreadCount` / `WORKER_THREADS` for your CPU; tune `setMaxQueueSize` / `MAX_QUEUE_SIZE` for maximum simultaneous clients
