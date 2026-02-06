# Configuration Guide

## Overview

Geruest supports flexible configuration through multiple sources with a clear hierarchy:

**Configuration Hierarchy (highest to lowest priority):**
1. **Code** - Explicit setter method calls (e.g., `server.setPort(8080)`)
2. **.env file** - Values from `.env` file
3. **Environment variables** - System environment variables

This means values set via code will **never** be overridden by .env or environment variables.

## Usage

### Loading Configuration

```cpp
#include <geruest/Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    // Load configuration from .env file and environment variables
    // By default, looks for ".env" in the current working directory
    server.loadConfig();  // Loads from ".env" in current directory
    
    // Or specify a custom path (e.g., same directory as executable)
    server.loadConfig("/path/to/.env");
    
    // Any explicit setter calls take precedence over config
    server.setPort(8080);  // This will override PORT in .env
    
    server.init();
    server.start();
    
    return 0;
}
```

**Note:** Place your `.env` file in the same directory as your executable or specify the full path when calling `loadConfig()`.

### Configuration Priority Example

```cpp
// Example demonstrating the hierarchy
geruest::Geruest server;

// 1. Values from .env or environment variables
server.loadConfig();  // Loads PORT=3000 from .env

// 2. Code overrides config
server.setPort(8080);  // This takes precedence over .env

// Final value: PORT = 8080 (from code, not from .env)
```

## Supported Configuration Keys

### Server Configuration

| Key        | Type   | Default     | Description           |
| ---------- | ------ | ----------- | --------------------- |
| `PORT`     | int    | 8080        | Server listening port |
| `HOSTNAME` | string | "localhost" | Server hostname       |

### Image Processing

| Key               | Type  | Default | Description                                 |
| ----------------- | ----- | ------- | ------------------------------------------- |
| `WEBP_CONVERSION` | bool  | false   | Enable automatic PNG/JPG to WebP conversion |
| `WEBP_QUALITY`    | float | 75      | WebP encoding quality (0-100)               |

### Asset Management

| Key            | Type | Default | Description                           |
| -------------- | ---- | ------- | ------------------------------------- |
| `MERGE_ASSETS` | bool | false   | Enable automatic CSS/JS asset merging |

### Development Settings

| Key         | Type   | Default | Description                                            |
| ----------- | ------ | ------- | ------------------------------------------------------ |
| `DEV_MODE`  | bool   | false   | Enable development mode (verbose logging, no caching)  |
| `LOG_LEVEL` | string | "error" | Log level: "none", "error", "warning", "info", "debug" |

### Threading Configuration

| Key              | Type   | Default       | Description                   |
| ---------------- | ------ | ------------- | ----------------------------- |
| `WORKER_THREADS` | size_t | CPU cores × 2 | Number of worker threads      |
| `MAX_QUEUE_SIZE` | size_t | 500           | Maximum connection queue size |

### Email Configuration (SMTP)

| Key                 | Type   | Default  | Description                                               |
| ------------------- | ------ | -------- | --------------------------------------------------------- |
| `SMTP_SERVER`       | string | -        | SMTP server hostname (e.g., "smtp.gmail.com")             |
| `SMTP_PORT`         | int    | 587      | SMTP server port (587=TLS, 465=SSL, 25=unencrypted)       |
| `SMTP_USERNAME`     | string | -        | SMTP authentication username (usually your email)         |
| `SMTP_PASSWORD`     | string | -        | SMTP authentication password (use app password for Gmail) |
| `SMTP_FROM_ADDRESS` | string | username | Email "From" address (defaults to username if not set)    |
| `SMTP_USE_TLS`      | bool   | true     | Require TLS encryption; if TLS cannot be established, sending fails (no fallback to plaintext) |

**⚠️ SECURITY WARNING:** When `SMTP_USE_TLS=true` (default), the email system **requires** TLS encryption and will **fail to send** if TLS cannot be established. This prevents silent downgrade attacks where credentials and email content could be transmitted in cleartext. Setting `SMTP_USE_TLS=false` allows unencrypted SMTP connections (insecure - only use for local testing).

### Email Spam Protection

| Key                       | Type   | Default | Description                                 |
| ------------------------- | ------ | ------- | ------------------------------------------- |
| `EMAIL_MIN_INTERVAL`      | int    | 60      | Minimum seconds between emails from same IP |
| `EMAIL_MAX_PER_IP`        | size_t | 10      | Maximum emails per IP in tracking window    |
| `EMAIL_TRACKING_DURATION` | int    | 3600    | Seconds to track IP activity (1 hour)       |
| `EMAIL_MAX_QUEUE_SIZE`    | size_t | 1000    | Maximum pending emails in queue             |

## .env File Format

Create a `.env` file in your project root:

```env
# Server Configuration
PORT=8080
HOSTNAME=localhost

# WebP Image Conversion
WEBP_CONVERSION=true
WEBP_QUALITY=85

# Development Settings
DEV_MODE=false
MERGE_ASSETS=true

# Threading Configuration
WORKER_THREADS=8
MAX_QUEUE_SIZE=1000

# Logging
LOG_LEVEL=info

# Email Configuration (SMTP)
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password
SMTP_FROM_ADDRESS=noreply@example.com
SMTP_USE_TLS=true

# Email Spam Protection
EMAIL_MIN_INTERVAL=60
EMAIL_MAX_PER_IP=10
EMAIL_TRACKING_DURATION=3600
EMAIL_MAX_QUEUE_SIZE=1000
```

### Format Rules

- Lines starting with `#` are comments
- Format: `KEY=value`
- Whitespace around `=` is ignored
- Quotes are optional: `KEY="value"` or `KEY=value`
- Empty lines are ignored

### Boolean Values

Recognized as `true`:
- `true`, `1`, `yes`, `on` (case-insensitive)

Recognized as `false`:
- `false`, `0`, `no`, `off` (case-insensitive)

## Environment Variables

Configuration can also be loaded from system environment variables:

```bash
# Linux/macOS
export PORT=8080
export DEV_MODE=true
export LOG_LEVEL=debug
./my_server

# Windows (PowerShell)
$env:PORT=8080
$env:DEV_MODE="true"
.\my_server.exe

# Windows (CMD)
set PORT=8080
set DEV_MODE=true
my_server.exe
```

## Priority Examples

### Example 1: Code Overrides Everything

```cpp
// .env file contains: PORT=3000
// Environment has: export PORT=4000

geruest::Geruest server;
server.loadConfig();        // Would load 3000 from .env (not 4000 from env)
server.setPort(8080);       // Code takes precedence
// Final: PORT = 8080
```

### Example 2: .env Overrides Environment

```cpp
// .env file contains: DEV_MODE=true
// Environment has: export DEV_MODE=false

geruest::Geruest server;
server.loadConfig();
// Final: DEV_MODE = true (from .env, not environment)
```

### Example 3: Environment Variable Fallback

```cpp
// .env file does NOT contain PORT
// Environment has: export PORT=9000

geruest::Geruest server;
server.loadConfig();
// Final: PORT = 9000 (from environment)
```

### Example 4: Mixed Configuration

```cpp
// .env file:
// PORT=3000
// DEV_MODE=true

// Environment:
// export WORKER_THREADS=16

geruest::Geruest server;
server.loadConfig();          // Loads PORT and DEV_MODE from .env
                              // Loads WORKER_THREADS from environment
server.setPort(8080);         // Overrides PORT from code
server.setWebPQuality(90);    // Set via code (not in config)

// Final configuration:
// PORT = 8080 (code)
// DEV_MODE = true (.env)
// WORKER_THREADS = 16 (environment)
// WEBP_QUALITY = 90 (code)
```

## Best Practices

### 1. Load Configuration Early

```cpp
int main() {
    geruest::Geruest server;
    
    // Load config BEFORE other setup
    server.loadConfig();
    
    // Then perform any code-based overrides
    if (isProductionEnv()) {
        server.setLogLevel(geruest::LogLevel::Warning);
    }
    
    server.init();
    server.start();
}
```

### 2. Use .env for Development

Keep sensitive or environment-specific values in `.env` (and add it to `.gitignore`):

```env
# .env (local development)
DEV_MODE=true
LOG_LEVEL=debug
PORT=3000

# Email for testing
SMTP_SERVER=smtp.gmail.com
SMTP_USERNAME=test@gmail.com
SMTP_PASSWORD=your-app-password
```

### 3. Use Environment Variables for Production

In production, use environment variables for security and flexibility:

```bash
# Production deployment
export PORT=80
export LOG_LEVEL=warning
export WORKER_THREADS=32
export SMTP_SERVER=smtp.sendgrid.net
export SMTP_USERNAME=apikey
export SMTP_PASSWORD=$SENDGRID_API_KEY
./server
```

### 4. Use Code for Defaults and Logic

Use explicit setters for:
- Application defaults
- Conditional logic
- Critical security settings

```cpp
server.loadConfig();

// Override based on runtime conditions
if (argc > 1 && std::string(argv[1]) == "--debug") {
    server.enableDevMode();
}

// Force security settings in code
if (isProduction()) {
    server.setLogLevel(geruest::LogLevel::Error);
}
```

## Email Configuration Guide

### Gmail Setup

Gmail requires **App Passwords** for SMTP authentication (regular password won't work with 2FA):

1. **Enable 2-Factor Authentication** on your Google account
2. **Generate App Password**:
   - Go to: https://myaccount.google.com/apppasswords
   - Select "Mail" and your device
   - Copy the 16-character password
3. **Add to .env**:

```env
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=abcd efgh ijkl mnop  # 16-char app password
SMTP_FROM_ADDRESS=your-email@gmail.com
SMTP_USE_TLS=true
```

### Other Email Providers

#### Microsoft 365 / Outlook

```env
SMTP_SERVER=smtp.office365.com
SMTP_PORT=587
SMTP_USERNAME=your-email@outlook.com
SMTP_PASSWORD=your-password
SMTP_USE_TLS=true
```

#### SendGrid (Transactional Email Service)

```env
SMTP_SERVER=smtp.sendgrid.net
SMTP_PORT=587
SMTP_USERNAME=apikey
SMTP_PASSWORD=SG.xxxxxxxxxxxxxxxxxxxxxxxxxxxxx
SMTP_FROM_ADDRESS=noreply@yourdomain.com
SMTP_USE_TLS=true
```

#### AWS SES (Amazon Simple Email Service)

```env
SMTP_SERVER=email-smtp.us-east-1.amazonaws.com
SMTP_PORT=587
SMTP_USERNAME=AKIAIOSFODNN7EXAMPLE
SMTP_PASSWORD=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
SMTP_FROM_ADDRESS=verified@yourdomain.com
SMTP_USE_TLS=true
```

### Email Security Recommendations

1. **Never commit .env files with real credentials**
2. **Use app-specific passwords** (Gmail, Yahoo, etc.)
3. **For production**, use environment variables or secrets management
4. **Enable TLS** (`SMTP_USE_TLS=true`) for all connections
5. **Configure spam protection** to prevent abuse:

```env
EMAIL_MIN_INTERVAL=60      # At least 60s between emails per IP
EMAIL_MAX_PER_IP=5         # Max 5 emails per IP per hour
EMAIL_TRACKING_DURATION=3600  # Track IPs for 1 hour
```

## Best Practices

### 1. Load Configuration Early

```cpp
int main() {
    geruest::Geruest server;
    
    // Load config BEFORE other setup
    server.loadConfig();
    
    // Then perform any code-based overrides
    if (isProductionEnv()) {
        server.setLogLevel(geruest::LogLevel::Warning);
    }
    
    server.init();
    server.start();
}
```

### 2. Use .env for Development

Keep sensitive or environment-specific values in `.env` (and add it to `.gitignore`):

```env
# .env (local development)
DEV_MODE=true
LOG_LEVEL=debug
PORT=3000
```

### 3. Use Environment Variables for Production

In production, use environment variables for security and flexibility:

```bash
# Production deployment
export PORT=80
export LOG_LEVEL=warning
export WORKER_THREADS=32
./server
```

### 4. Use Code for Defaults and Logic

Use explicit setters for:
- Application defaults
- Conditional logic
- Critical security settings

```cpp
server.loadConfig();

// Override based on runtime conditions
if (argc > 1 && std::string(argv[1]) == "--debug") {
    server.enableDevMode();
}

// Force security settings in code
if (isProduction()) {
    server.setLogLevel(geruest::LogLevel::Error);
}
```

## .gitignore Recommendation

Add `.env` to your `.gitignore` to avoid committing sensitive configuration:

```gitignore
# Environment variables
.env
.env.local
.env.*.local

# Keep example file for documentation
!.env.example
```

Then provide an `.env.example` file in your repository:

```env
# .env.example - Copy to .env and configure

# Server Configuration
PORT=8080
HOSTNAME=localhost

# Features
WEBP_CONVERSION=false
MERGE_ASSETS=false
DEV_MODE=false

# Threading
WORKER_THREADS=8
MAX_QUEUE_SIZE=500

# Logging
LOG_LEVEL=error
```

## ConfigLoader Direct API

You can also use the `ConfigLoader` class directly in your application code:

```cpp
#include <geruest/config/ConfigLoader.hpp>

// Load .env file
geruest::ConfigLoader::loadEnvFile(".env");

// Get values with fallback hierarchy
std::string dbHost = geruest::ConfigLoader::get("DB_HOST", "localhost");
int dbPort = geruest::ConfigLoader::getInt("DB_PORT", 5432);
bool enableCache = geruest::ConfigLoader::getBool("ENABLE_CACHE", true);
float timeout = geruest::ConfigLoader::getFloat("TIMEOUT", 30.0f);

// Check if key exists
if (geruest::ConfigLoader::has("API_KEY")) {
    std::string apiKey = geruest::ConfigLoader::get("API_KEY");
}
```

This allows you to use the same configuration system for your own application settings beyond just the Geruest server configuration.

## See Also

- [Getting Started Guide](GETTING_STARTED.md)
- [Usage Guide](USAGE_GUIDE.md)
- [Development Mode](DEV_MODE.md)
