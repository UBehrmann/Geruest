# Features Overview

## Quick Reference

| Feature | Description | Key Method/Header |
|---------|-------------|-------------------|
| **Routing** | Exact paths, wildcards (`/api/*`, `/users/*/profile`) | `addRoute()` |
| **Static Files** | Auto-serve files from root directories | `addRoot()` |
| **Templates** | Component injection `{file}`, translations `[key]` | `ContentBuilder` |
| **Asset Merging** | Combine CSS/JS files from file maps | `AssetMerger` |
| **Translations** | Multi-language with JSON files | `setAvailableLanguages()` |
| **CORS** | Preflight/CORS headers | `setCORSHeaders()` |
| **Basic Auth** | SHA-256 password protection | `BasicAuth` |
| **HTTPS/TLS** | SSL/TLS encryption | `enableTLS()` |
| **Email/SMTP** | Send emails via SMTP | `EmailService` |
| **JSON Parsing** | String-based JSON handling | `JSONParser` |
| **WebP Conversion** | Auto-convert images to WebP | `WebPConverter` |
| **Configuration** | `.env` and programmatic config | `configManager` |

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

## Multi-Language

```cpp
server.setAvailableLanguages({"en", "de", "fr"});  // First = default
```

URLs get language prefixes: `/about` → `/en/about`, `/de/about`, `/fr/about`

## CORS Support

```cpp
server.setCORSHeaders({
    {"Access-Control-Allow-Origin", "*"},
    {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"}
});
```

## Basic Authentication

```cpp
#include <geruest/auth/BasicAuth.hpp>

BasicAuth auth;
auth.addUser("admin", BasicAuth::hashPassword("secret123"));

server.addRoute("/admin", [&auth](const HTTPRequest& req) {
    if (!auth.authenticate(req)) {
        return auth.respondUnauthorized("Admin Area");
    }
    return HTTPResponse("200 OK", "Admin Dashboard");
});
```

**Always use HTTPS with authentication!**

## HTTPS/TLS

```cpp
server.enableTLS("cert.pem", "key.pem");
server.start(443);
```

## Email Service

```cpp
#include <geruest/email/EmailService.hpp>

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
#include <geruest/builders/WebPConverter.hpp>

WebPConverter::convertToWebP("input.png", "output.webp", 90);  // Quality: 90%
```

## Configuration System

**`.env` file:**
```env
SERVER_PORT=8080
TLS_ENABLED=true
SMTP_HOST=smtp.gmail.com
```

**Programmatic:**
```cpp
configManager.set("SERVER_PORT", "8080");
int port = configManager.getInt("SERVER_PORT", 80);  // Default: 80
```

## Advanced Patterns

**Graceful Shutdown:**
```cpp
std::atomic<bool> running{true};
signal(SIGINT, [](int) { running = false; });

server.start(8080, [&running]() { return running.load(); });
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
