# Getting Started

Quick installation and first server setup for Geruest C++ web framework.

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
#include <geruest/Geruest.hpp>

int main() {
    using namespace geruest;
    
    Geruest server;
    
    server.addRoute("/", [](const HTTPRequest& req) {
        HTTPResponse res("200 OK");
        res.setBody("<h1>Hello, World!</h1>");
        return res;
    });
    
    server.start(8080);
    return 0;
}
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.17)
project(MyServer)
set(CMAKE_CXX_STANDARD 17)

find_package(Geruest REQUIRED)
add_executable(myserver main.cpp)
target_link_libraries(myserver PRIVATE Geruest::Geruest)
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
server.start(8080);
```

## Complete Example

**main.cpp with API routes and static serving:**
```cpp
#include <geruest/Geruest.hpp>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nShutting down gracefully...\n";
    running = false;
}

int main() {
    using namespace geruest;
    
    signal(SIGINT, signalHandler);
    
    Geruest server;
    
    // Static files
    server.addRoot("/var/www/mysite");
    
    // API routes
    server.addRoute("/api/status", [](const HTTPRequest& req) {
        HTTPResponse res("200 OK");
        res.setHeader("Content-Type", "application/json");
        res.setBody(R"({"status":"online","version":"1.0.0"})");
        return res;
    });
    
    server.addRoute("/api/users", [](const HTTPRequest& req) {
        HTTPResponse res("200 OK");
        res.setHeader("Content-Type", "application/json");
        res.setBody(R"({"users":[]})");
        return res;
    });
    
    // CORS for API
    server.setCORSHeaders({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"}
    });
    
    std::cout << "Server running on http://localhost:8080\n";
    std::cout << "Press Ctrl+C to stop\n";
    
    server.start(8080, [&running]() { return running.load(); });
    
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
server.start(8081);  // Try different port
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
