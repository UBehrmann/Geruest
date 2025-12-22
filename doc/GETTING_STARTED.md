# Getting Started with Geruest

Geruest is a modern, lightweight C++17 web framework designed for building high-performance HTTP servers with cross-platform compatibility.

## Table of Contents

- [Requirements](#requirements)
- [Installation](#installation)
  - [Linux](#linux)
  - [Windows (MSVC)](#windows-msvc)
  - [Windows (MinGW)](#windows-mingw)
- [Quick Start](#quick-start)
- [Project Structure](#project-structure)
- [Your First Server](#your-first-server)

---

## Requirements

### System Requirements

- **C++17** compatible compiler
- **CMake** 3.10 or higher
- **Operating System**: Linux, Windows, or macOS

### Supported Compilers

| Platform | Compiler | Minimum Version |
|----------|----------|-----------------|
| Linux    | GCC      | 7.0+            |
| Linux    | Clang    | 5.0+            |
| Windows  | MSVC     | 2017+           |
| Windows  | MinGW-w64| 7.0+            |
| macOS    | Clang    | 5.0+            |

### Dependencies

Geruest has **no external dependencies** beyond the C++ standard library. Everything is included:

- **Threading**: `std::thread` (C++11 standard library)
- **Filesystem**: `std::filesystem` (C++17 standard library)
- **Network**: Platform-specific socket libraries (included with OS)
  - Windows: WinSock2 (included with Windows SDK)
  - Linux/macOS: POSIX sockets (included with OS)
- **JSON**: Custom implementation (no external libraries)

---

## Installation

### Linux

```bash
# Clone the repository
git clone https://github.com/yourusername/Geruest.git
cd Geruest

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Install (optional, may require sudo)
make install
```

The library will be installed to:
- Headers: `/usr/local/include/geruest/`
- Library: `/usr/local/lib/libGeruest.a`
- CMake config: `/usr/local/lib/cmake/Geruest/`

### Windows (MSVC)

```powershell
# Clone the repository
git clone https://github.com/yourusername/Geruest.git
cd Geruest

# Create build directory
mkdir build
cd build

# Configure with CMake (Visual Studio generator)
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Install
cmake --install . --config Release
```

### Windows (MinGW)

```powershell
# Clone the repository
git clone https://github.com/yourusername/Geruest.git
cd Geruest

# Create build directory
mkdir build
cd build

# Configure with CMake (MinGW Makefiles)
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build .

# Install
cmake --install .
```

### Using as a Subproject

You can also include Geruest directly in your CMake project:

```cmake
# In your CMakeLists.txt
add_subdirectory(path/to/Geruest)
target_link_libraries(your_target PRIVATE Geruest)
```

---

## Quick Start

### Minimal Example

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    // Configure server
    server.setPort(8080);
    server.setHostname("localhost");
    
    // Add a simple route
    server.addRoute("/hello", [](const geruest::HTTPRequest& req) {
        geruest::HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/plain");
        response.setBody("Hello, World!");
        return response;
    });
    
    // Start the server
    server.init();
    server.start();
    
    return 0;
}
```

### CMakeLists.txt for Your Project

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyWebApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Geruest (if installed)
find_package(Geruest REQUIRED)

add_executable(mywebapp main.cpp)
target_link_libraries(mywebapp PRIVATE Geruest::Geruest)

# On Windows, link WinSock
if(WIN32)
    target_link_libraries(mywebapp PRIVATE ws2_32)
endif()

# Link pthread on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(mywebapp PRIVATE pthread)
endif()
```

---

## Project Structure

### Recommended Website Structure

```
your_project/
├── CMakeLists.txt
├── main.cpp
└── website/
    ├── assets/
    │   ├── css/
    │   │   ├── base.css
    │   │   └── layout.css
    │   ├── js/
    │   │   ├── utils.js
    │   │   └── main.js
    │   ├── images/
    │   │   └── logo.png
    │   └── translations/
    │       ├── en.json
    │       └── de.json
    ├── components/
    │   ├── header.html
    │   ├── footer.html
    │   └── navigation.html
    └── html/
        ├── index.html
        ├── about.html
        └── contact.html
```

### Directory Purposes

| Directory | Purpose |
|-----------|---------|
| `assets/css/` | Stylesheets (auto-merged when enabled) |
| `assets/js/` | JavaScript files (auto-merged when enabled) |
| `assets/images/` | Static images |
| `assets/translations/` | JSON translation files |
| `components/` | Reusable HTML components (for injection) |
| `html/` | Main HTML templates |

---

## Your First Server

### Complete Example with Static Files

```cpp
#include <Geruest.hpp>
#include <csignal>
#include <filesystem>
#include <iostream>

using namespace geruest;

Geruest* server = nullptr;

// Graceful shutdown handler
void signalHandler(int signum) {
    std::cout << "Shutting down server..." << std::endl;
    if (server) server->stop();
}

int main(int argc, char* argv[]) {
    server = new Geruest();
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Basic configuration
    server->setPort(8080);
    server->setHostname("localhost");
    
    // Optional: Configure thread pool
    server->setWorkerThreadCount(8);  // Number of worker threads
    server->setMaxQueueSize(500);     // Max pending connections
    
    // Optional: Set up languages
    server->setAvailableLanguages({"en", "de", "fr"});
    
    // Optional: Enable CSS/JS merging
    server->setMergeAssets(true);
    
    // Set the website root directory
    std::filesystem::path websitePath = 
        std::filesystem::canonical(argv[0]).parent_path() / "website";
    server->addRoot(websitePath.string());
    
    // Add API routes
    server->addRoute("/api/status", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"status": "running", "version": "1.0.0"})");
        return response;
    });
    
    // Add wildcard route
    server->addRoute("/api/*", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"message": "API endpoint", "path": ")" + 
                         req.getPathString() + R"("})");
        return response;
    });
    
    std::cout << "Starting server on http://localhost:8080" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Initialize and start
    server->init();
    server->start();
    
    // Cleanup
    delete server;
    return 0;
}
```

### Build and Run

```bash
# Linux
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./mywebapp

# Windows (MSVC)
mkdir build && cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
.\Release\mywebapp.exe
```

---

## Next Steps

- [Usage Guide](USAGE_GUIDE.md) - Detailed usage instructions with Docker examples
- [Features](FEATURES.md) - Overview of all features
- [Data Classes](DATA_CLASSES.md) - HTTPRequest, HTTPResponse, JSONParser documentation
- [Basic Authentication](BASIC_AUTH.md) - Protect your pages
- [Translations](TRANSLATIONS.md) - Multi-language support
- [HTML Injections](HTML_INJECTIONS.md) - Component system
- [Asset Merging](ASSET_MERGING.md) - CSS/JS optimization

---

## Troubleshooting

### Common Issues

#### "Cannot find Geruest" CMake error
Make sure you've installed the library or set `CMAKE_PREFIX_PATH`:
```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/geruest/install
```

#### "bind: Address already in use"
Another process is using the port. Either stop it or use a different port:
```cpp
server->setPort(3000);  // Use a different port
```

#### Permission denied on Linux (port 80)
Ports below 1024 require root privileges. Either:
- Use a port above 1024 (e.g., 8080)
- Run with sudo (not recommended for production)
- Use a reverse proxy like nginx

#### Windows Firewall blocking connections
Allow your application through Windows Firewall or use `localhost` for local testing.
