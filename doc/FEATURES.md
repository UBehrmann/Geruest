# Features Overview

Geruest provides a comprehensive set of features for building modern web servers. This document gives an overview of all features with usage examples.

## Table of Contents

- [Routing System](#routing-system)
- [Static File Serving](#static-file-serving)
- [Thread Pool](#thread-pool)
- [Multi-Language Support](#multi-language-support)
- [Asset Merging](#asset-merging)
- [HTML Component Injection](#html-component-injection)
- [Translation Injection](#translation-injection)
- [Basic Authentication](#basic-authentication)
- [JSON Parser](#json-parser)
- [Graceful Shutdown](#graceful-shutdown)

---

## Routing System

Geruest supports both exact routes and wildcard pattern matching.

### Exact Routes

```cpp
// Simple GET route
server.addRoute("/hello", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("Hello, World!");
    return response;
});

// JSON API endpoint
server.addRoute("/api/users", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setBody(R"({"users": [{"id": 1, "name": "John"}]})");
    return response;
});
```

### Wildcard Routes

Wildcards use `*` to match any characters in a path segment.

```cpp
// Match any path under /api/
server.addRoute("/api/*", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"path": ")" + req.getPathString() + R"("})");
    return response;
});
// Matches: /api/users, /api/posts/123, /api/anything

// User profile pattern: /users/{id}/profile
server.addRoute("/users/*/profile", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"profile": true, "path": ")" + req.getPathString() + R"("})");
    return response;
});
// Matches: /users/123/profile, /users/john/profile

// File extension pattern
server.addRoute("/downloads/*.zip", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setBody("ZIP download requested");
    return response;
});
// Matches: /downloads/file.zip, /downloads/archive.zip

// Multiple wildcards
server.addRoute("/static/*/images/*", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setBody("Static image requested");
    return response;
});
// Matches: /static/v1/images/logo.png, /static/v2/images/header.jpg
```

### Route Priority

1. **Exact routes** are checked first (O(1) hash lookup)
2. **Wildcard routes** are checked second (O(n) pattern matching)
3. **Static files** are served if no route matches
4. **404 response** if nothing matches

### HTTP Methods

The request method is available via `req.getMethod()`:

```cpp
server.addRoute("/api/resource", [](const HTTPRequest& req) {
    std::string method = req.getMethod();
    
    if (method == "GET") {
        return responseOK();
    } else if (method == "POST") {
        // Handle POST
        std::string body = req.getBody();
        return responseCreated();
    } else if (method == "PUT") {
        return responseOK();
    } else if (method == "DELETE") {
        return responseNoContent();
    }
    
    return responseMethodNotAllowed();
});
```

---

## Static File Serving

Set a root directory and Geruest automatically serves files from it.

```cpp
server.addRoot("/path/to/website");
```

### Supported Content Types

| Extension    | Content-Type           |
| ------------ | ---------------------- |
| `.html`      | text/html              |
| `.css`       | text/css               |
| `.js`        | application/javascript |
| `.json`      | application/json       |
| `.png`       | image/png              |
| `.jpg/.jpeg` | image/jpeg             |
| `.gif`       | image/gif              |
| `.svg`       | image/svg+xml          |
| `.webp`      | image/webp             |
| `.ico`       | image/x-icon           |
| `.woff`      | font/woff              |
| `.woff2`     | font/woff2             |
| `.ttf`       | font/ttf               |
| `.pdf`       | application/pdf        |

### URL to File Mapping

| URL Request        | Served File                      |
| ------------------ | -------------------------------- |
| `/`                | `/html/index.html`               |
| `/about`           | `/html/about.html`               |
| `style.css`        | `/assets/css/style.css`          |
| `subdir/style.css` | `/assets/css/subdir/style.css`   |
| `script.js`        | `/assets/js/script.js`           |
| `subdir/script.js` | `/assets/js/subdir/script.js`    |
| `logo.png`         | `/assets/images/logo.png`        |
| `subdir/logo.png`  | `/assets/images/subdir/logo.png` |

---

## Thread Pool

Geruest uses a thread pool for handling concurrent connections efficiently.

### Configuration

```cpp
// Set number of worker threads (default: CPU cores × 2)
server.setWorkerThreadCount(16);

// Set maximum pending connection queue size (default: 500)
server.setMaxQueueSize(1000);
```

### Recommended Configurations

| Scenario     | Workers | Queue Size |
| ------------ | ------- | ---------- |
| Development  | 4       | 100        |
| General use  | CPU × 2 | 500        |
| High traffic | 32+     | 2000+      |
| Embedded     | 2-4     | 50-100     |

### How It Works

1. Main thread accepts incoming connections
2. Connections are queued
3. Worker threads pick up connections from the queue
4. Each worker processes one request at a time
5. Workers are reused for subsequent requests

---

## Multi-Language Support

Geruest supports automatic language routing and translation.

### Setup

```cpp
// Set available languages (first is default)
server.setAvailableLanguages({"en", "de", "fr"});
```

### How It Works

1. URLs are automatically prefixed with language codes
2. `href="/about"` becomes `href="/en/about"` 
3. Template pages are processed with language-specific translations
4. New language specific pages are generated under `/lang_code/` folders
5. Static assets (CSS, JS, images) are not prefixed

### Directory Structure

```
website/
├── html/
│   ├── index.html         # Template (shared)
│   ├── en/
│   │   └── index.html     # Generated for English
│   ├── de/
│   │   └── index.html     # Generated for German
│   └── fr/
│       └── index.html     # Generated for French
└── assets/
    └── translations/
        ├── en.json
        ├── de.json
        └── fr.json
```

See [TRANSLATIONS.md](TRANSLATIONS.md) for detailed documentation.

---

## Asset Merging

Automatically merge multiple CSS and JS files into single files per page.

### Enable Asset Merging

```cpp
server.setMergeAssets(true);  // Must be called before init()
```

### How It Works

**Before (multiple HTTP requests):**
```html
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="layout.css">
<link rel="stylesheet" href="page.css">

<script src="utils.js"></script>
<script src="api.js"></script>
<script src="main.js"></script>
```

**After (single HTTP requests):**
```html
<link rel="stylesheet" href="/index.css">
<script src="/index.js"></script>
```

### Benefits

- Reduces HTTP requests (6 → 2 in example above)
- Automatic - no configuration files needed
- Files are regenerated when HTML templates change
- JavaScript is wrapped in IIFE to prevent scope pollution

See [ASSET_MERGING.md](ASSET_MERGING.md) for detailed documentation.

---

## HTML Component Injection

Include reusable HTML components in your templates.

### Syntax

Use curly braces `{component_path}` to include components:

```html
<!DOCTYPE html>
<html>
<head>
    <title>My Page</title>
</head>
<body>
    {components/header.html}
    
    <main>
        <h1>Welcome</h1>
        <p>Page content here</p>
    </main>
    
    {components/footer.html}
</body>
</html>
```

### Component Files

```html
<!-- components/header.html -->
<header>
    <nav>
        <a href="/">Home</a>
        <a href="/about">About</a>
        <a href="/contact">Contact</a>
    </nav>
</header>
```

```html
<!-- components/footer.html -->
<footer>
    <p>&copy; 2025 My Company</p>
</footer>
```

See [HTML_INJECTIONS.md](HTML_INJECTIONS.md) for detailed documentation.

---

## Translation Injection

Inject translated strings from JSON files.

### Syntax

Use square brackets `[path/to/file.json:key]`:

```html
<h1>[assets/translations/main.json:welcome_title]</h1>
<p>[assets/translations/main.json:welcome_message]</p>
```

### Translation Files

```json
// assets/translations/main.json
{
    "en": {
        "welcome_title": "Welcome!",
        "welcome_message": "Thanks for visiting our site."
    },
    "de": {
        "welcome_title": "Willkommen!",
        "welcome_message": "Danke für Ihren Besuch."
    }
}
```

See [TRANSLATIONS.md](TRANSLATIONS.md) for detailed documentation.

---

## Basic Authentication

Protect specific pages with HTTP Basic Authentication.

### Setup

```cpp
// Enable authentication
server.setBasicAuthEnabled(true);

// Add users
server.addBasicAuthUser("admin", "secret123");
server.addBasicAuthUser("editor", "password456");

// Protect pages
server.addProtectedPage("/admin");
server.addProtectedPage("/api/admin");
server.addProtectedPage("/dashboard");
```

### How It Works

1. User requests protected page
2. Server sends `401 Unauthorized` with `WWW-Authenticate` header
3. Browser shows login dialog
4. User credentials are sent as Base64-encoded `Authorization` header
5. Server validates credentials (SHA-256 hashed passwords)
6. Access granted or denied

### Features

- SHA-256 password hashing
- Pre-hashed password support for config files
- Path-based protection
- Automatic bypass when disabled

See [BASIC_AUTH.md](BASIC_AUTH.md) for detailed documentation.

---

## JSON Parser

Built-in JSON parser with type-safe accessors.

### Parsing JSON

```cpp
#include "parser/JSONParser.hpp"

// From string
JSONParser json(R"({"name": "John", "age": 30, "active": true})");

// From file
JSONParser* json = getJSONFromFile("config.json");
```

### Getting Values

```cpp
std::string name = json.getString("name");     // "John"
int age = json.getInt("age");                  // 30
bool active = json.getBool("active");          // true
double price = json.getDouble("price");        // 19.99

// Nested objects
JSONParser user = json.getObject("user");
std::string email = user.getString("email");

// Arrays
std::vector<std::string> tags = json.getStringArray("tags");
std::vector<int> numbers = json.getIntArray("numbers");
std::vector<JSONParser> items = json.getArrayOfJSON("items");
```

### Creating JSON

```cpp
JSONParser json;

json.setString("name", "John");
json.setInt("age", 30);
json.setBool("active", true);
json.setDouble("price", 19.99);

// Arrays
json.setStringArray("tags", {"cpp", "web", "server"});
json.setIntArray("scores", {95, 87, 92});

// Output
std::string output = json.toString();
// {"name":"John","age":30,"active":true,"price":19.99,...}
```

See [DATA_CLASSES.md](DATA_CLASSES.md) for complete API reference.

---

## Graceful Shutdown

Properly shut down the server when receiving signals.

### Signal Handler Setup

```cpp
#include <csignal>

std::unique_ptr<Geruest> server;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    if (server) {
        server->stop();
    }
}

int main() {
    server = std::make_unique<Geruest>();
    
    // Register signal handlers
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request
    
    server->setPort(8080);
    server->init();
    server->start();  // Blocks until stop() is called
    
    // Automatic cleanup when unique_ptr goes out of scope
    return 0;
}
```

### Shutdown Process

1. Signal received (SIGINT/SIGTERM)
2. `server->stop()` called
3. Server stops accepting new connections
4. Worker threads finish current requests
5. Thread pool shuts down
6. Socket is closed
7. `start()` returns

### Checking Server Status

```cpp
if (server->isRunning()) {
    std::cout << "Server is running" << std::endl;
}
```

---

## Feature Comparison

| Feature         | Default   | Configurable              |
| --------------- | --------- | ------------------------- |
| Port            | 8080      | `setPort()`               |
| Hostname        | localhost | `setHostname()`           |
| Worker Threads  | CPU × 2   | `setWorkerThreadCount()`  |
| Queue Size      | 500       | `setMaxQueueSize()`       |
| Languages       | None      | `setAvailableLanguages()` |
| Asset Merging   | Off       | `setMergeAssets()`        |
| Basic Auth      | Off       | `setBasicAuthEnabled()`   |
| Comment Removal | On        | Via ServerData            |

---

## Next Steps

- [Data Classes](DATA_CLASSES.md) - HTTPRequest, HTTPResponse, JSONParser reference
- [HTML Injections](HTML_INJECTIONS.md) - Component system details
- [Translations](TRANSLATIONS.md) - Multi-language system
- [Basic Authentication](BASIC_AUTH.md) - Security setup
- [Asset Merging](ASSET_MERGING.md) - CSS/JS optimization
