# Getting Started

Quick installation and first server setup for Geruest C++ web framework.

## Requirements

- **C++20** compiler (GCC 10+, Clang 11+, MSVC with C++20)
- **CMake** 3.10 or newer (3.17+ recommended for `find_package` CONFIG patterns)
- **Boost** 1.75+ with **Boost.System** (Asio uses it unless you build against header-only Boost as in the library’s FetchContent path). Examples:
  - Debian/Ubuntu: `sudo apt-get install libboost-system-dev`
  - Fedora: `sudo dnf install boost-devel`
  - MSYS2: `pacman -S mingw-w64-x86_64-boost`
  - vcpkg: `vcpkg install boost-system`
- Optional: **libcurl** (email), **libwebp** (image conversion) — see main README

## Installation

### Linux
```bash
git clone https://github.com/yourusername/Geruest.git
cd Geruest
chmod +x setup_scripts/linux_setup.sh
./setup_scripts/linux_setup.sh
```

### Windows
```powershell
git clone https://github.com/yourusername/Geruest.git
cd Geruest
setup_scripts\windows_setup.bat
```

### Manual Build

**Linux/Unix:**
```bash
# Install Boost (Debian/Ubuntu example) if not already present
sudo apt-get install -y libboost-system-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

**Windows (MSVC):**
```powershell
mkdir build && cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --config Release
```

## Quick Start

**Minimal Server (main.cpp):**
```cpp
#include <Geruest.hpp>

int main() {
    using namespace geruest;
    
    Geruest server;
    server.setPort(8080);
    server.setHostname("localhost");
    
    server.addRoute("/", [](const HTTPRequest& req) {
        HTTPResponse res("200 OK");
        res.setBody("<h1>Hello, World!</h1>");
        return res;
    });
    
    server.init();
    server.start();
    return 0;
}
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.17)
project(MyServer)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.75 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(Geruest REQUIRED)
add_executable(myserver main.cpp)
target_link_libraries(myserver PRIVATE Geruest::Geruest Boost::system Threads::Threads)
```

**Build & Run:**
```bash
mkdir build && cd build
cmake .. && cmake --build .
./myserver  # Server on http://localhost:8080
```

## Project Structure

**Recommended Layout:**
```
my_project/
├── CMakeLists.txt
├── main.cpp
├── .env                    # Configuration (optional)
└── website/
    ├── html/
    │   └── index.html
    ├── assets/
    │   ├── css/
    │   ├── js/
    │   └── images/
    └── components/
        ├── header.html
        └── footer.html
```

**With Static Files:**
```cpp
server.addRoot("/path/to/website");  // Serves all files automatically
server.setPort(8080);
server.init();
server.start();
```

## Complete Example

**main.cpp with API routes and static serving:**
```cpp
#include <Geruest.hpp>
#include <csignal>
#include <memory>

std::unique_ptr<geruest::Geruest> server;

void signalHandler(int signal) {
    std::cout << "\nShutting down gracefully...\n";
    if (server) {
        server->stop();
    }
}

int main() {
    using namespace geruest;
    
    std::signal(SIGINT, signalHandler);
    
    server = std::make_unique<Geruest>();
    
    // Static files
    server->addRoot("/var/www/mysite");
    
    // API routes
    server->addRoute("/api/status", [](const HTTPRequest& req) {
        HTTPResponse res("200 OK");
        res.setHeader("Content-Type", "application/json");
        res.setHeader("Access-Control-Allow-Origin", "*");
        res.setBody(R"({"status":"online","version":"1.0.0"})");
        return res;
    });
    
    server->addRoute("/api/users", [](const HTTPRequest& req) {
        HTTPResponse res("200 OK");
        res.setHeader("Content-Type", "application/json");
        res.setHeader("Access-Control-Allow-Origin", "*");
        res.setBody(R"({"users":[]})");
        return res;
    });
    
    std::cout << "Server running on http://localhost:8080\n";
    std::cout << "Press Ctrl+C to stop\n";
    
    server->setPort(8080);
    server->setHostname("localhost");
    server->init();
    server->start();
    
    return 0;
}
```

## Next Steps

1. **Static Serving**: See [USAGE_GUIDE.md](USAGE_GUIDE.md) for `addRoot()` details
2. **Templates**: Read [HTML_INJECTIONS.md](HTML_INJECTIONS.md) for component system
3. **Translations**: See [TRANSLATIONS.md](TRANSLATIONS.md) for multi-language support
4. **Asset Bundling**: Check [ASSET_MERGING.md](ASSET_MERGING.md) for CSS/JS combining
5. **Authentication**: Read [BASIC_AUTH.md](BASIC_AUTH.md) for password protection
6. **Configuration**: See [CONFIGURATION.md](CONFIGURATION.md) for `.env` files
7. **Features Overview**: Browse [FEATURES.md](FEATURES.md) for all capabilities

## Troubleshooting

**Library not found:**
```bash
# Linux
export CMAKE_PREFIX_PATH=/path/to/install
# Or set CMAKE_PREFIX_PATH in CMakeLists.txt

cmake .. -DCMAKE_PREFIX_PATH=/usr/local
```

**Port already in use:**
```cpp
server.setPort(8081);  // Try different port
server.init();
server.start();
```

**Permission denied (ports < 1024):**
```bash
sudo ./myserver           # Run with sudo for port 80/443
# OR use capability: sudo setcap 'cap_net_bind_service=+ep' ./myserver
```

**Cross-platform builds:**
- Use `#ifdef _WIN32` for Windows-specific code
- Always test with both MSVC and GCC/Clang
- Check socket cleanup: `WSACleanup()` on Windows, `close()` on Unix
