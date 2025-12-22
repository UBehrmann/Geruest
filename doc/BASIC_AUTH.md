# Basic Authentication

Geruest provides built-in HTTP Basic Authentication to protect specific pages. This is a lightweight access control system suitable for admin panels, internal tools, or staging environments.

## Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Configuration API](#configuration-api)
- [Password Security](#password-security)
- [Examples](#examples)
- [Advanced Usage](#advanced-usage)
- [Security Considerations](#security-considerations)

---

## Overview

### What is Basic Authentication?

HTTP Basic Authentication is a simple authentication scheme built into the HTTP protocol:

1. Client requests a protected resource
2. Server responds with `401 Unauthorized` and `WWW-Authenticate` header
3. Browser shows a login dialog
4. User enters credentials
5. Browser sends credentials encoded in `Authorization` header
6. Server validates and grants/denies access

### Features

- **SHA-256 password hashing** - Passwords are never stored in plain text
- **Path-based protection** - Protect specific paths
- **Pre-hashed password support** - Load hashes from config files
- **Enable/disable toggle** - Quickly enable or disable authentication
- **Automatic bypass** - Disabled when no users configured

### Limitations

- Credentials sent with every request (Base64 encoded, not encrypted)
- No session management
- Browser caches credentials until closed
- **Always use HTTPS in production** for security

---

## Quick Start

### Basic Setup

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    server.setPort(8080);
    server.addRoot("/path/to/website");
    
    // 1. Enable authentication
    server.setBasicAuthEnabled(true);
    
    // 2. Add users
    server.addBasicAuthUser("admin", "secret123");
    server.addBasicAuthUser("editor", "password456");
    
    // 3. Protect pages
    server.addProtectedPage("/admin");
    server.addProtectedPage("/dashboard");
    
    server.init();
    server.start();
    
    return 0;
}
```

Now:
- `/admin` and `/dashboard` require login
- All other pages are public
- Users can log in with `admin:secret123` or `editor:password456`

---

## Configuration API

### Enabling/Disabling Authentication

```cpp
// Enable authentication (required for protection to work)
server.setBasicAuthEnabled(true);

// Disable authentication (all pages become public)
server.setBasicAuthEnabled(false);
```

### Managing Users

```cpp
// Add user with plain text password (will be SHA-256 hashed)
server.addBasicAuthUser("username", "password");

// Add user with pre-hashed password (64 hex characters)
server.addBasicAuthUserHashed("username", "5e884898da28047d91089d48a...");

// Remove a user
bool removed = server.removeBasicAuthUser("username");

// Clear all users
server.clearBasicAuthUsers();
```

### Managing Protected Pages

```cpp
// Protect a page
server.addProtectedPage("/admin");
server.addProtectedPage("/api/admin");
server.addProtectedPage("/dashboard/settings");

// Unprotect a page
bool removed = server.removeProtectedPage("/admin");

// Clear all protected pages
server.clearProtectedPages();
```

### Static Password Hashing

Generate hashes for config files:

```cpp
// Generate SHA-256 hash
std::string hash = geruest::Geruest::hashPassword("mypassword");
// Returns: "89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8"
```

---

## Password Security

### How Passwords Are Stored

Passwords are **never stored in plain text**. When you call `addBasicAuthUser()`:

1. Password is hashed using SHA-256
2. Only the hash is stored in memory
3. During authentication, submitted password is hashed and compared

```cpp
// Both of these create the same stored hash
server.addBasicAuthUser("admin", "secret123");
// Internally stores: SHA256("secret123") = "fcf730b6d95236ecd3c9..."

// Using pre-hashed
server.addBasicAuthUserHashed("admin", "fcf730b6d95236ecd3c9...");
```

### Pre-Hashed Passwords

For production, generate hashes offline and use `addBasicAuthUserHashed()`:

```cpp
// Generate hash once
std::cout << Geruest::hashPassword("mySecurePassword") << std::endl;
// Output: 89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8

// Use in code (password never appears in source)
server.addBasicAuthUserHashed("admin", 
    "89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8");
```

---

## Examples

### Admin Panel Protection

```cpp
#include <Geruest.hpp>
#include <csignal>
#include <iostream>

using namespace geruest;

Geruest* server = nullptr;

void signalHandler(int signum) {
    if (server) server->stop();
}

int main() {
    server = new Geruest();
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    server->setPort(8080);
    server->addRoot("./website");
    
    // Enable auth
    server->setBasicAuthEnabled(true);
    
    // Add admin user
    server->addBasicAuthUser("admin", "admin123");
    
    // Protect admin pages
    server->addProtectedPage("/admin");
    server->addProtectedPage("/admin/users");
    server->addProtectedPage("/admin/settings");
    
    // Public routes remain accessible
    server->addRoute("/", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/html");
        response.setBody("<h1>Public Home Page</h1>");
        return response;
    });
    
    // Protected admin route
    server->addRoute("/admin", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/html");
        response.setBody("<h1>Admin Dashboard</h1><p>You are authenticated!</p>");
        return response;
    });
    
    server->init();
    server->start();
    
    delete server;
    return 0;
}
```

### API Endpoint Protection

```cpp
// Protect specific API endpoints
server->addProtectedPage("/api/admin");
server->addProtectedPage("/api/users/create");
server->addProtectedPage("/api/users/delete");

// Public API endpoints remain accessible
server->addRoute("/api/status", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"status": "online"})");
    return response;
});

// Protected admin API
server->addRoute("/api/admin/stats", [](const HTTPRequest& req) {
    // Only accessible after authentication
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"users": 100, "requests": 5000})");
    return response;
});
```

### Loading Users from Config File

```cpp
#include <Geruest.hpp>
#include "parser/JSONParser.hpp"

void loadUsersFromConfig(Geruest* server, const std::string& configPath) {
    JSONParser* config = getJSONFromFile(configPath);
    if (!config) {
        std::cerr << "Could not load config file" << std::endl;
        return;
    }
    
    // Config format:
    // {
    //     "auth_enabled": true,
    //     "users": [
    //         {"username": "admin", "password_hash": "abc123..."},
    //         {"username": "editor", "password_hash": "def456..."}
    //     ],
    //     "protected_pages": ["/admin", "/dashboard"]
    // }
    
    bool authEnabled = config->getBool("auth_enabled");
    server->setBasicAuthEnabled(authEnabled);
    
    std::vector<JSONParser> users = config->getArrayOfJSON("users");
    for (const auto& user : users) {
        std::string username = user.getString("username");
        std::string hash = user.getString("password_hash");
        server->addBasicAuthUserHashed(username, hash);
    }
    
    std::vector<std::string> pages = config->getStringArray("protected_pages");
    for (const auto& page : pages) {
        server->addProtectedPage(page);
    }
    
    delete config;
}

int main() {
    Geruest server;
    
    server.setPort(8080);
    server.addRoot("./website");
    
    loadUsersFromConfig(&server, "config/auth.json");
    
    server.init();
    server.start();
    
    return 0;
}
```

### Config File Example (`config/auth.json`)

```json
{
    "auth_enabled": true,
    "users": [
        {
            "username": "admin",
            "password_hash": "89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8"
        },
        {
            "username": "editor",
            "password_hash": "5e884898da28047d9108e4fc1f3e5be92eff8e3dd2e2ad0c1e5f7a88ff7c8c87"
        }
    ],
    "protected_pages": [
        "/admin",
        "/admin/users",
        "/admin/settings",
        "/dashboard",
        "/api/admin"
    ]
}
```

### Multiple Protection Levels

```cpp
// Set up different user types
server.addBasicAuthUser("superadmin", "super123");
server.addBasicAuthUser("admin", "admin123");
server.addBasicAuthUser("editor", "editor123");

// All users can access these
server.addProtectedPage("/protected");

// Create route handler that checks user type
server.addRoute("/admin/superonly", [](const HTTPRequest& req) {
    // Check authorization header for specific user
    std::string authHeader = req.getHeader("Authorization");
    
    // Decode and check username (simplified example)
    // In production, you might want a more robust solution
    
    if (authHeader.find("superadmin") == std::string::npos) {
        return responseForbidden();
    }
    
    HTTPResponse response("200 OK");
    response.setBody("Super admin content");
    return response;
});
```

---

## Advanced Usage

### Custom 401 Response

The server automatically sends `401 Unauthorized` with a `WWW-Authenticate` header. The default response looks like:

```http
HTTP/1.1 401 Unauthorized
WWW-Authenticate: Basic realm="Restricted Area"
Content-Type: text/plain
Content-Length: 12

Unauthorized
```

### Wildcard Path Protection

```cpp
// Protect all paths starting with /admin
// Note: Geruest checks exact paths, so list all or use route handlers
server.addProtectedPage("/admin");
server.addProtectedPage("/admin/users");
server.addProtectedPage("/admin/settings");
server.addProtectedPage("/admin/reports");

// For programmatic protection of patterns, use route handlers:
server.addRoute("/admin/*", [](const HTTPRequest& req) {
    // This route is reached AFTER authentication check
    // Authentication is already validated at this point
    HTTPResponse response("200 OK");
    response.setBody("Admin area: " + req.getPathString());
    return response;
});
```

### Environment-Based Configuration

```cpp
#include <cstdlib>

int main() {
    Geruest server;
    
    // Check environment
    const char* env = std::getenv("APP_ENV");
    bool isProd = env && std::string(env) == "production";
    
    if (isProd) {
        // Production: load from secure config
        server.setBasicAuthEnabled(true);
        
        const char* adminHash = std::getenv("ADMIN_PASSWORD_HASH");
        if (adminHash) {
            server.addBasicAuthUserHashed("admin", adminHash);
        }
    } else {
        // Development: simple credentials or disabled
        server.setBasicAuthEnabled(false);
        // Or use simple dev credentials:
        // server.setBasicAuthEnabled(true);
        // server.addBasicAuthUser("dev", "dev");
    }
    
    server.addProtectedPage("/admin");
    
    // ... rest of setup
}
```

### Hash Generation Utility

Create a simple utility to generate password hashes:

```cpp
// hash_password.cpp
#include <Geruest.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: hash_password <password>" << std::endl;
        return 1;
    }
    
    std::string password = argv[1];
    std::string hash = geruest::Geruest::hashPassword(password);
    
    std::cout << "Password: " << password << std::endl;
    std::cout << "SHA-256:  " << hash << std::endl;
    
    return 0;
}
```

```bash
# Build and run
g++ -std=c++17 hash_password.cpp -o hash_password -lGeruest
./hash_password "mySecurePassword"
# Output:
# Password: mySecurePassword
# SHA-256:  89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8
```

---

## Security Considerations

### Always Use HTTPS

Basic Authentication sends credentials Base64-encoded (NOT encrypted). Without HTTPS:

- Credentials can be intercepted
- Anyone on the network can read passwords
- Replay attacks are possible

**Always deploy with HTTPS in production!**

Use a reverse proxy like nginx with SSL:

```nginx
server {
    listen 443 ssl;
    server_name example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### Password Best Practices

1. **Use strong passwords** - Minimum 12 characters, mixed case, numbers, symbols
2. **Never commit passwords** - Use environment variables or external config
3. **Use pre-hashed passwords** in production code
4. **Rotate credentials** regularly

### When NOT to Use Basic Auth

Basic Auth is suitable for:
- Admin panels
- Internal tools
- Staging/preview environments
- Simple API protection

**Not suitable for**:
- Public-facing user authentication
- Applications requiring sessions
- High-security applications
- Mobile apps (can't control credential caching)

For complex authentication needs, implement:
- JWT tokens
- OAuth 2.0
- Session-based authentication
- Third-party auth providers

### Path Considerations

Be explicit about protected paths:

```cpp
// Protect specific paths
server.addProtectedPage("/admin");           // Protects /admin only
server.addProtectedPage("/admin/users");     // Protects /admin/users

// Note: /admin/other is NOT protected unless explicitly added
```

---

## Troubleshooting

### Authentication Not Working

1. **Check if enabled**: `setBasicAuthEnabled(true)`
2. **Check users exist**: At least one user must be added
3. **Check path matches**: Path must exactly match protected page

### Browser Not Asking for Credentials

1. Browser may have cached credentials - close and reopen
2. Check the path is actually protected
3. Verify server is sending 401 response

### "Access Denied" After Correct Login

1. Verify password hash matches
2. Check for typos in username
3. Try clearing browser cache

### How to Log Out

Basic Auth has no built-in logout. Options:

1. Close the browser (clears cached credentials)
2. Use a "logout" URL that returns 401 (forces re-authentication)
3. Use JavaScript to redirect with invalid credentials

---

## Next Steps

- [Features](FEATURES.md) - All features overview
- [Usage Guide](USAGE_GUIDE.md) - Deployment with nginx/HTTPS
- [Contributing](CONTRIBUTING.md) - Contribute to Geruest
