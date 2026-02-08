# Configuration System

Priority-based configuration with `.env` files and programmatic overrides.

## Configuration Priority

**Highest → Lowest:**
1. **Programmatic** (`configManager.set()`)
2. **Environment Variables** (`export VAR=value`)
3. **`.env` File** (workspace root)
4. **Defaults** (second parameter in `get*()` methods)

## .env File Format

```env
# Server Configuration
SERVER_PORT=8080
SERVER_HOST=0.0.0.0
TLS_ENABLED=true
TLS_CERT_PATH=/etc/ssl/cert.pem
TLS_KEY_PATH=/etc/ssl/key.pem

# SMTP Configuration
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USER=user@gmail.com
SMTP_PASS=app-password
SMTP_USE_TLS=true

# Security
ADMIN_USERNAME=admin
ADMIN_PASSWORD_HASH=5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8

# Development
DEV_MODE=false
LOG_LEVEL=INFO
```

## API Reference

```cpp
#include <geruest/config/ConfigurationManager.hpp>

// Read values (with defaults)
int port = configManager.getInt("SERVER_PORT", 80);
float timeout = configManager.getFloat("REQUEST_TIMEOUT", 30.0);
bool tlsEnabled = configManager.getBool("TLS_ENABLED", false);
std::string host = configManager.getString("SERVER_HOST", "127.0.0.1");

// Set values (overrides everything)
configManager.set("SERVER_PORT", "8080");
configManager.set("TLS_ENABLED", "true");

// Check existence
bool hasKey = configManager.has("SMTP_HOST");

// Get all keys
auto keys = configManager.getAllKeys();  // Returns std::vector<std::string>
```

## Common Patterns

### Server Configuration

```cpp
int port = configManager.getInt("SERVER_PORT", 8080);
std::string host = configManager.getString("SERVER_HOST", "0.0.0.0");

if (configManager.getBool("TLS_ENABLED", false)) {
    server.enableTLS(
        configManager.getString("TLS_CERT_PATH"),
        configManager.getString("TLS_KEY_PATH")
    );
    server.start(port);
}
```

### SMTP Setup

```cpp
if (configManager.has("SMTP_HOST")) {
    EmailService email(
        serverData,
        configManager.getString("SMTP_HOST"),
        configManager.getInt("SMTP_PORT", 587),
        configManager.getString("SMTP_USER"),
        configManager.getString("SMTP_PASS")
    );
}
```

### Authentication

```cpp
BasicAuth auth;
std::string passwordHash = configManager.getString("ADMIN_PASSWORD_HASH");
auth.addUser("admin", passwordHash);  // Pre-hashed password from .env
```

### Development Mode

```cpp
bool devMode = configManager.getBool("DEV_MODE", false);

if (devMode) {
    sendToLogger("Development mode enabled");
    server.setRemoveComments(false);  // Keep HTML comments
}
```

## Security Best Practices

**Never commit `.env` to version control:**
```bash
echo ".env" >> .gitignore
```

**Use environment variables in production:**
```bash
export SERVER_PORT=443
export TLS_ENABLED=true
./my_server
```

**Store hashed passwords only:**
```cpp
// Generate hash
std::string hash = BasicAuth::hashPassword("my-secret-password");
// Store hash in .env: ADMIN_PASSWORD_HASH=<hash>
```

**Use different `.env` per environment:**
```
.env.development
.env.staging
.env.production
```

Load with: `ln -sf .env.production .env`

## Docker Configuration

**docker-compose.yml:**
```yaml
services:
  app:
    image: myapp
    environment:
      - SERVER_PORT=8080
      - TLS_ENABLED=true
      - SMTP_HOST=smtp.gmail.com
    env_file:
      - .env.production
```

## Type Conversion Rules

```cpp
// parseInt(), parseFloat() - throws on invalid input
getInt("PORT", 80);      // "8080" → 8080, "invalid" throws
getFloat("TIMEOUT", 1.5); // "3.14" → 3.14f
getBool("ENABLED", false); // "true"/"1" → true, "false"/"0" → false
getString("NAME", "");    // Returns raw string value
```

## Complete Example

```cpp
#include <geruest/Geruest.hpp>
#include <geruest/config/ConfigurationManager.hpp>
#include <geruest/auth/BasicAuth.hpp>

int main() {
    using namespace geruest;
    
    // Configuration already loaded from .env automatically
    int port = configManager.getInt("SERVER_PORT", 8080);
    bool tlsEnabled = configManager.getBool("TLS_ENABLED", false);
    
    Geruest server;
    
    if (tlsEnabled) {
        server.enableTLS(
            configManager.getString("TLS_CERT_PATH"),
            configManager.getString("TLS_KEY_PATH")
        );
    }
    
    // Auth from config
    BasicAuth auth;
    auth.addUser(
        configManager.getString("ADMIN_USERNAME", "admin"),
        configManager.getString("ADMIN_PASSWORD_HASH")
    );
    
    server.addRoute("/admin", [&auth](const HTTPRequest& req) {
        if (!auth.authenticate(req)) {
            return auth.respondUnauthorized("Admin");
        }
        return HTTPResponse("200 OK", "Admin Dashboard");
    });
    
    server.start(port);
    return 0;
}
```

## Troubleshooting

- **Values not loading**: Check `.env` is in workspace root (where executable runs)
- **Type conversion errors**: Ensure `.env` values match expected types
- **Overrides not working**: Remember priority order (programmatic > env vars > .env)
- **Missing required keys**: Use `has()` to check existence before `get*()`
