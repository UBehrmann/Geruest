# Geruest Documentation

Welcome to the Geruest documentation! This guide will help you get started with the framework and understand all its features.

## Quick Navigation

### Getting Started

- [**Getting Started**](GETTING_STARTED.md) - Prerequisites, installation, and your first server
- [**Usage Guide**](USAGE_GUIDE.md) - Local development, Docker deployment, Linux/Unix setup
- [**Configuration**](CONFIGURATION.md) - .env files, environment variables, code setters, configuration hierarchy

### Core Features

- [**Features Overview**](FEATURES.md) - Complete feature documentation with examples
- [**Redirects and 404**](REDIRECTS_AND_404.md) - Redirect maps, wildcard redirects, and custom 404 pages
- [**Data Classes**](DATA_CLASSES.md) - HTTPRequest, HTTPResponse, JSONParser API reference
- [**Database Usage**](DATABASE.md) - Async DB setup and route usage (PostgreSQL/SQLite)
- [**WebSockets**](WEBSOCKETS.md) - `addRouteWebSocket` coroutine and callback APIs

### Template System

- [**HTML Injections**](HTML_INJECTIONS.md) - Component inclusion system
- [**Translations**](TRANSLATIONS.md) - Multi-language support and injection
- [**Asset Merging**](ASSET_MERGING.md) - Automatic CSS/JS bundling

### Security

- [**Security Utilities**](SECURITY.md) - SQL injection, XSS, JSON injection, path traversal
- [**Basic Authentication**](BASIC_AUTH.md) - HTTP Basic Auth for protected pages
- [**Gated Routes & Pages**](GATED_ROUTES_PAGES.md) - `addGatedPage()`, `addGatedPageAsync()`, `addGatedRoute()`, `addGatedRouteAsync()`

### Development

- [**Development Mode**](DEV_MODE.md) - Verbose logging and no-cache mode for rapid development

### Contributing

- [**Contributing Guide**](CONTRIBUTING.md) - Development setup, code style, testing
- [**Project Review**](PROJECT_REVIEW.md) - Architecture, DX, feature scope, and code quality review (June 2025)

---

## Quick Start

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    server.setPort(8080);
    server.setHostname("localhost");
    server.addRoot("./website");
    
    server.addRoute("/hello", [](const geruest::HTTPRequest& req) {
        geruest::HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/plain");
        response.setBody("Hello, World!");
        return response;
    });
    
    server.init();
    server.start();
    
    return 0;
}
```

## Feature Summary

| Feature | Description | Documentation |
|---------|-------------|---------------|
| **Routing** | Exact and wildcard route patterns | [Features](FEATURES.md#routing-system) |
| **Redirects** | Exact/wildcard redirects and redirect maps | [Redirects and 404](REDIRECTS_AND_404.md) |
| **Static Files** | Automatic file serving | [Features](FEATURES.md#static-file-serving) |
| **404 Page** | Custom file-backed not-found handling | [Redirects and 404](REDIRECTS_AND_404.md#custom-404-page) |
| **Thread pool + async I/O** | Boost.Asio `io_context`, async accept, per-connection strand | [Features](FEATURES.md#server-io-and-thread-pool) |
| **Languages** | Multi-language URL routing | [Translations](TRANSLATIONS.md) |
| **Asset Merging** | CSS/JS bundling per page | [Asset Merging](ASSET_MERGING.md) |
| **Components** | Reusable HTML includes | [HTML Injections](HTML_INJECTIONS.md) |
| **Translations** | JSON-based i18n | [Translations](TRANSLATIONS.md) |
| **Security** | SQL injection, XSS, JSON injection, path traversal | [Security](SECURITY.md) |
| **Basic Auth** | HTTP authentication | [Basic Auth](BASIC_AUTH.md) |
| **Dev Mode** | Verbose logs, no file caching | [Dev Mode](DEV_MODE.md) |
| **JSON Parser** | Built-in JSON handling | [Data Classes](DATA_CLASSES.md#jsonparser) |
| **Async routes** | C++20 coroutines via `addRouteAsync` | [Features](FEATURES.md#routing) |
| **Database** | Optional PostgreSQL/SQLite async backends | [Database](DATABASE.md) |
| **WebSockets** | RFC 6455 persistent connections | [WebSockets](WEBSOCKETS.md) |

## Requirements

- **C++20** compatible compiler (GCC 10+, Clang 11+)
- **CMake** 3.10+
- **Boost** (1.75+): Asio + Boost.System for the HTTP server (see [Getting Started](GETTING_STARTED.md#requirements))
- Optional: **libpq** / **libsqlite3** (database backends), **libcurl** (email), **libwebp** (WebP conversion)

See [Getting Started](GETTING_STARTED.md) for detailed requirements.

## License

See [LICENSE](../LICENSE) in the repository root.
