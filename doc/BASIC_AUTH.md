# Basic Authentication

HTTP Basic Authentication for protecting pages with SHA-256 password hashing.

## Quick Start

```cpp
server.setBasicAuthEnabled(true);
server.addBasicAuthUser("admin", "secret123");  // SHA-256 hashed internally
server.addProtectedPage("/admin");
server.addProtectedPage("/dashboard");
```

All `/admin` and `/dashboard` pages now require login.

## API

```cpp
// Enable/disable
void setBasicAuthEnabled(bool enabled);

// User management
void addBasicAuthUser(const std::string& username, const std::string& password);
void addBasicAuthUserHashed(const std::string& username, const std::string& hash);  // 64-char hex
bool removeBasicAuthUser(const std::string& username);
void clearBasicAuthUsers();

// Protected pages
void addProtectedPage(const std::string& path);
bool removeProtectedPage(const std::string& path);
void clearProtectedPages();

// Utility
static std::string hashPassword(const std::string& password);  // Get SHA-256 hash
```

## Pre-Hashed Passwords

For production, generate hashes offline:
```cpp
// Generate hash once
std::cout << Geruest::hashPassword("myPassword") << std::endl;
// Output: 89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8

// Use hashed version
server.addBasicAuthUserHashed("admin", "89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8");
```

## Example: Admin Panel

```cpp
server.setBasicAuthEnabled(true);
server.addBasicAuthUser("admin", "admin123");
server.addProtectedPage("/admin");
server.addProtectedPage("/admin/users");
server.addProtectedPage("/api/admin");

// Public routes accessible
server.addRoute("/", [](const HTTPRequest& req) {
    return responseOK();  // No auth needed
});

// Protected route (auth validated before handler called)
server.addRoute("/admin", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setBody("<h1>Admin Dashboard</h1>");
    return response;
});
```

## Load from Config

```cpp
// config.json:
// {
//   "auth_enabled": true,
//   "users": [{"username":"admin", "password_hash":"89e0153..."}],
//   "protected_pages": ["/admin", "/dashboard"]
// }

JSONParser* config = getJSONFromFile("config.json");
server.setBasicAuthEnabled(config->getBool("auth_enabled"));

for (const auto& user : config->getArrayOfJSON("users")) {
    server.addBasicAuthUserHashed(user.getString("username"), user.getString("password_hash"));
}

for (const auto& page : config->getStringArray("protected_pages")) {
    server.addProtectedPage(page);
}
delete config;
```

## Security

**⚠️ ALWAYS USE HTTPS IN PRODUCTION**

Basic Auth sends Base64-encoded credentials (NOT encrypted). Without HTTPS:
- Credentials can be intercepted
- Replay attacks possible

**Best Practices:**
1. Always use HTTPS
2. Use strong passwords (12+ chars, mixed)
3. Never commit passwords - use env vars or external config
4. Use pre-hashed passwords in code
5. Rotate credentials regularly

**Use Basic Auth for:**
- Admin panels
- Internal tools
- Staging environments
- Simple API protection

**DON'T use for:**
- Public-facing user auth
- High-security applications
- Mobile apps (credential caching issues)

**Deploy with nginx + SSL:**
```nginx
server {
    listen 443 ssl;
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    location / {
        proxy_pass http://127.0.0.1:8080;
    }
}
```