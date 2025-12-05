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

## Run

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

2. **Change the port** (recommended): Edit `exemple.cpp` and change `PORT` to a value > 1024 (e.g., 8080):
   ```cpp
   #define PORT 8080
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