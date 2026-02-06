# Geruest Example Server

This example demonstrates how to use the Geruest web framework to create a simple HTTP server with routing capabilities.

## Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler (GCC on Linux, MinGW on Windows)
- Geruest library installed

## Build Instructions

### Linux

```bash
rm -rf build
mkdir -p build
cd build
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Windows (MinGW)

```powershell
# Clean previous build (optional)
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# Create build directory
mkdir build
cd build

# Configure and build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

## Configuration

The example server uses a `.env` file for configuration. Create a `.env` file in the same directory as your executable:

```bash
# Copy the example configuration
cp ../../.env.example .env

# Edit with your settings
nano .env  # or your preferred editor
```

### Minimal Configuration

For basic functionality without email:

```env
PORT=8080
LOG_LEVEL=info
```

### Email Configuration (Optional)

To enable email functionality, add SMTP settings:

```env
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password
SMTP_FROM_ADDRESS=noreply@example.com
SMTP_USE_TLS=true
```

**Note**: If email settings are not provided, email functionality will be disabled but the server will run normally. See [Configuration Guide](../doc/CONFIGURATION.md) for all available options.

## Build Instructions

### Linux

```bash
rm -rf build
mkdir -p build
cd build
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Windows (MinGW)

```powershell
# Clean previous build (optional)
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# Create build directory
mkdir build
cd build

# Configure and build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

## Run

### Configuration First

Before running, create or copy the `.env` file to the build directory where the executable is located:

```bash
# From the build directory
cp ../../.env.example .env
# Edit .env with your settings (PORT, email config, etc.)
```

### Linux

```bash
# From the build directory
sudo ./exemple
```

**Note on Port 80 (Linux)**: The example uses port 80 by default, which requires root privileges on Linux. You have two options:

1. **Run with sudo** (not recommended for development):
   ```bash
   sudo ./exemple
   ```

2. **Change the port in .env** (recommended):
   ```env
   PORT=8080
   ```
   Then run without sudo:
   ```bash
   ./exemple
   ```

### Windows

```bash
# From the build directory
./exemple.exe
```

## Features Demonstrated

- Basic HTTP server setup
- Static file serving from `website/` directory
- Route registration with exact matches
- Wildcard routing patterns (`/api/*`, `/users/*/profile`)
- Graceful shutdown with Ctrl+C
- Cross-platform compatibility

## Accessing the Server

Once running, the server will be accessible at:
- `http://localhost:80` (or your configured port)

### Example Routes

- `/test` - Simple test page
- `/api/get` - GET endpoint example
- `/api/post` - POST endpoint example
- `/api/*` - Wildcard route for any API path
- `/users/*/profile` - User profile pattern
- `/downloads/*.zip` - File extension matching
- `/static/*/images/*` - Multi-level wildcard

## Stopping the Server

Press `Ctrl+C` to gracefully stop the server.

```