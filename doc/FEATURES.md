# Features Overview

## Quick Reference

| Feature | Description | Key Method/Header |
|---------|-------------|-------------------|
| **Routing** | Exact paths, wildcards (`/api/*`, `/users/*/profile`) | `addRoute()` |
| **Static Files** | Auto-serve files from root directories | `addRoot()` |
| **Templates** | Component injection `{file}`, translations `[key]` | `ContentBuilder` |
| **Asset Merging** | Combine CSS/JS files from file maps | `AssetMerger` |
| **JS Obfuscation** | Make JavaScript harder to analyze | `setObfuscationLevel()` |
| **Translations** | Multi-language with JSON files | `setAvailableLanguages()` |
| **CORS** | Preflight/CORS headers | Manual headers |
| **Basic Auth** | SHA-256 password protection | `BasicAuth` |
| **Email/SMTP** | Send emails via SMTP (with TLS) | `EmailService` |
| **JSON Parsing** | String-based JSON handling | `JSONParser` |
| **WebP Conversion** | Auto-convert images to WebP | `WebPConverter` |
| **Configuration** | `.env` and environment config | `ConfigLoader` |

## Routing

```cpp
// Exact match (O(1) lookup)
server.addRoute("/api/users", [](const HTTPRequest& req) {
    HTTPResponse res("200 OK");
    res.setBody(R"({"users":[]})");
    return res;
});

// Wildcards (O(n) pattern matching)
server.addRoute("/users/*/profile", routeHandler);  // /users/123/profile
server.addRoute("/api/*", apiHandler);              // /api/anything
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

**`files_maps/css_file_map.json`:**
```json
{
    "bundle_name": "main.css",
    "files": ["reset.css", "layout.css"]
}
```

Automatic path normalization: `/css/main.css`, `/assets/css/main.css` → same bundle

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

```cpp
// Add CORS headers to individual responses
server.addRoute("/api/data", [](const HTTPRequest& req) {
    HTTPResponse res("200 OK");
    res.setHeader("Content-Type", "application/json");
    res.setHeader("Access-Control-Allow-Origin", "*");
    res.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
    res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.setBody(R"({"data": []})");
    return res;
});
```

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

## Email Service

```cpp
#include <email/EmailService.hpp>

EmailService email(serverData, "smtp.gmail.com", 587, "user@gmail.com", "app-password");
email.sendEmail("to@example.com", "Subject", "Message body");
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
server.set404Handler([](const HTTPRequest& req) {
    HTTPResponse res("404 Not Found");
    res.setBody("<h1>Page Not Found</h1>");
    return res;
});
```

**Request Logging:**
```cpp
server.addRoute("/api/*", [](const HTTPRequest& req) {
    sendToLoggerAPI(req.getMethod() + " " + req.getPath());
    // ... handle request ...
});
```

## Performance Tips

- **Route Lookup**: Prefer exact routes over wildcards (O(1) vs O(n))
- **Static Files**: Bypass routing entirely for better performance
- **Asset Merging**: Reduces HTTP requests dramatically
- **WebP**: Smaller image sizes, faster loading
- **Threading**: One thread per connection, handles concurrency automatically
