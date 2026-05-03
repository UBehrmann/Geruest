# Configuration System

Priority-based configuration with `.env` files and programmatic overrides.

## Configuration Priority

**Highest → Lowest:**
1. **`.env` File** (current working directory, or explicit path passed to `loadEnvFile()`)
2. **Environment Variables** (`export VAR=value`)
3. **Defaults** (second parameter in `get*()` methods)

**Note:** For `Geruest` server configuration, values explicitly set via setters (e.g., `server.setPort()`) take precedence over all configuration sources.

## .env File Format

```env
# Server Configuration
PORT=8080
HOSTNAME=0.0.0.0
WORKER_THREADS=16
# Max simultaneous TCP/HTTP sessions (not a pre-accept backlog queue)
MAX_QUEUE_SIZE=500
# Keep-alive request cap per connection (0 = unlimited)
MAX_REQUESTS_PER_CONNECTION=1000
LOG_LEVEL=error

# Database
DATABASE_BACKEND=none
DATABASE_POOL_MAX=4
SQLITE_PATH=./geruest.db
SQLITE_BUSY_TIMEOUT_MS=5000
SQLITE_DB_EXECUTOR_THREADS=1
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_DB=app
POSTGRES_USER=app
POSTGRES_PASSWORD=secret
POSTGRES_SSLMODE=prefer
POSTGRES_CONNECT_TIMEOUT=5
POSTGRES_STATEMENT_TIMEOUT_MS=30000
POSTGRES_PIPELINE_MAX_BATCH=8

# Feature Flags
DEV_MODE=false
MERGE_ASSETS=true
WEBP_CONVERSION=true
WEBP_QUALITY=75

# SMTP Configuration
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=user@gmail.com
SMTP_PASSWORD=app-password
SMTP_FROM_ADDRESS=noreply@example.com
SMTP_USE_TLS=true

# Email Spam Protection
EMAIL_MIN_INTERVAL=60
EMAIL_MAX_PER_IP=10
EMAIL_TRACKING_DURATION=3600
EMAIL_MAX_QUEUE_SIZE=1000

# Security (application-specific, not read by Geruest)
ADMIN_USERNAME=admin
ADMIN_PASSWORD_HASH=5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
```

## API Reference

```cpp
#include <config/ConfigLoader.hpp>

// Load .env file (optional - can also use server.loadConfig())
geruest::ConfigLoader::loadEnvFile(".env");

// Read values (with defaults)
int port = geruest::ConfigLoader::getInt("PORT", 8080);
float webpQuality = geruest::ConfigLoader::getFloat("WEBP_QUALITY", 75.0);
bool devMode = geruest::ConfigLoader::getBool("DEV_MODE", false);
std::string host = geruest::ConfigLoader::get("HOSTNAME", "localhost");
size_t maxConcurrentSessions = geruest::ConfigLoader::getSizeT("MAX_QUEUE_SIZE", 500);
size_t maxRequestsPerConnection = geruest::ConfigLoader::getSizeT("MAX_REQUESTS_PER_CONNECTION", 1000);

// Check existence
bool hasKey = geruest::ConfigLoader::has("SMTP_SERVER");

// Clear all loaded .env values
geruest::ConfigLoader::clear();
```

## Common Patterns

### Database backend selection

`DATABASE_BACKEND` selects runtime backend when both are compiled: `postgres`, `sqlite`, or `none`.
Selection precedence is:

1. explicit code setter (`setDatabaseBackend(...)`)
2. environment /.env `DATABASE_BACKEND`
3. default `none`

If the selected backend is not compiled in (`GERUEST_HAS_LIBPQ=0` or `GERUEST_HAS_SQLITE=0`), initialization throws. If the backend is compiled in but required settings are missing (empty `POSTGRES_DB`/`POSTGRES_USER` for Postgres, empty `SQLITE_PATH` for SQLite), Geruest logs an error and leaves the database client unset (`HTTPRequest::database()` stays null). See [DATABASE.md](DATABASE.md).

### Server Configuration

**`WORKER_THREADS`** controls how many threads (in addition to the thread that called `start()`) run **`boost::asio::io_context::run()`** for async I/O. **`MAX_QUEUE_SIZE`** is the **maximum number of concurrent client TCP sessions** the process will serve; extra connections are closed and appear in `/status` as `queue.rejections_total`. The JSON field `queue.current_size` is the current active session count. **`MAX_REQUESTS_PER_CONNECTION`** controls how many HTTP requests one keep-alive connection can serve before it is closed (`0` means unlimited, default `1000`).

```cpp
using namespace geruest;

Geruest server;

int port = ConfigLoader::getInt("PORT", 8080);
std::string host = ConfigLoader::get("HOSTNAME", "0.0.0.0");
size_t workers = ConfigLoader::getSizeT("WORKER_THREADS", 16);
size_t maxSessions = ConfigLoader::getSizeT("MAX_QUEUE_SIZE", 500);
size_t maxRequestsPerConnection = ConfigLoader::getSizeT("MAX_REQUESTS_PER_CONNECTION", 1000);

// Option 1: Manual configuration
server.setPort(port);
server.setHostname(host);
server.setWorkerThreadCount(workers);
server.setMaxQueueSize(maxSessions);
server.setMaxRequestsPerConnection(maxRequestsPerConnection);

// Option 2: Auto-load from .env (only loads unset values)
// This reads all Geruest-specific keys automatically
server.loadConfig(".env");

server.init();
server.start();
```

### SMTP Setup

```cpp
using namespace geruest;

// Option 1: Auto-initialize via server.loadConfig() (recommended)
Geruest server;
server.loadConfig(".env");  // Automatically initializes email if SMTP_* keys present

// Option 2: Manual initialization (overrides config)
if (ConfigLoader::has("SMTP_SERVER")) {
    EmailSender::Config emailConfig;
    emailConfig.smtpServer = ConfigLoader::get("SMTP_SERVER");
    emailConfig.port = ConfigLoader::getInt("SMTP_PORT", 587);
    emailConfig.username = ConfigLoader::get("SMTP_USERNAME");
    emailConfig.password = ConfigLoader::get("SMTP_PASSWORD");
    emailConfig.fromAddress = ConfigLoader::get("SMTP_FROM_ADDRESS", emailConfig.username);
    emailConfig.useTLS = ConfigLoader::getBool("SMTP_USE_TLS", true);
    EmailSender::init(emailConfig);
}
```

### Authentication

```cpp
using namespace geruest;

BasicAuth auth;
std::string passwordHash = ConfigLoader::get("ADMIN_PASSWORD_HASH");
auth.addUser("admin", passwordHash);  // Pre-hashed password from .env
```

### Development Mode

```cpp
using namespace geruest;

bool devMode = ConfigLoader::getBool("DEV_MODE", false);

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
export PORT=8080
export HOSTNAME=0.0.0.0
export DEV_MODE=false
export LOG_LEVEL=error
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
      - PORT=8080
      - HOSTNAME=0.0.0.0
      - SMTP_SERVER=smtp.gmail.com
      - SMTP_USERNAME=${SMTP_USERNAME}
      - SMTP_PASSWORD=${SMTP_PASSWORD}
    env_file:
      - .env.production
```

## Type Conversion Rules

```cpp
// Type conversion with defaults on invalid input
ConfigLoader::getInt("PORT", 8080);          // "8080" → 8080, "invalid" → 8080 (default)
ConfigLoader::getFloat("WEBP_QUALITY", 75.0); // "85.5" → 85.5f, "invalid" → 75.0
ConfigLoader::getBool("DEV_MODE", false);     // "true"/"1"/"yes"/"on" → true (case-insensitive)
                                              // "false"/"0"/"no"/"off" → false
ConfigLoader::get("HOSTNAME", "localhost");   // Returns raw string value
ConfigLoader::getSizeT("WORKER_THREADS", 8);  // "16" → 16, "invalid" → 8
```

## Complete Example

```cpp
#include <Geruest.hpp>
#include <config/ConfigLoader.hpp>
#include <auth/BasicAuth.hpp>

int main() {
    using namespace geruest;
    
    // Option 1: Auto-load all Geruest configuration (recommended)
    Geruest server;
    server.loadConfig(".env");  // Reads PORT, HOSTNAME, SMTP_*, etc.
    
    // Option 2: Manual configuration (overrides .env)
    // ConfigLoader::loadEnvFile(".env");
    // server.setPort(ConfigLoader::getInt("PORT", 8080));
    // server.setHostname(ConfigLoader::get("HOSTNAME", "0.0.0.0"));
    // server.setWorkerThreadCount(ConfigLoader::getSizeT("WORKER_THREADS", 8));
    
    // Application-specific config (not read by Geruest)
    BasicAuth auth;
    auth.addUser(
        ConfigLoader::get("ADMIN_USERNAME", "admin"),
        ConfigLoader::get("ADMIN_PASSWORD_HASH")
    );
    
    server.addRoute("/admin", [&auth](const HTTPRequest& req) {
        if (!auth.authenticate(req.getPathString(), req.getHeader("Authorization"))) {
            return responseUnauthorizedBasicAuth("Admin");
        }
        HTTPResponse response = responseOK();
        response.setBody("Admin Dashboard");
        return response;
    });
    
    server.init();
    server.start();
    return 0;
}
```

## Troubleshooting

- **Values not loading**: Check `.env` is in the current working directory (where the process runs) or provide an explicit path to `loadConfig()`/`loadEnvFile()`
- **Type conversion errors**: Invalid values return the default parameter, not errors
- **Priority confusion**: .env file takes precedence over environment variables
- **Missing required keys**: Use `ConfigLoader::has()` to check existence before retrieval
- **Server config not loading**: Call `server.loadConfig()` before `init()` or `start()`
