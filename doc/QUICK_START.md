# Geruest Library - Quick Start Guide

Welcome to Geruest! This guide provides simple copy-paste commands to build and install the Geruest library on your system.

## Prerequisites

Before you begin, ensure you have:
- **CMake 3.10+** (`cmake --version`)
- **C++17 compatible compiler** (GCC 7+, Clang 5+, or Visual Studio 2019+)
- **Git** (for cloning the repository)

## Quick Installation (One-Click Solutions)

### 🖥️ Windows (PowerShell)

**Complete installation with one command:**
```powershell
# Clone, build, and install Geruest in one go
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release; echo "🎉 Geruest installed successfully!"
```

**Alternative MinGW version:**
```powershell
# For MinGW users (if MSVC fails)
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue; mkdir build; cd build; cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release; cmake --build .; cmake --install .; echo "🎉 Geruest installed successfully!"
```

### 🐧 Linux/macOS (Bash)

**Complete installation with one command:**
```bash
# Clone, build, and install Geruest in one go
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && rm -rf build && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && sudo cmake --install . && echo "🎉 Geruest installed successfully!"
```

**Local installation (no sudo required):**
```bash
# Install to ~/.local instead of system-wide
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && rm -rf build && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && cmake --install . --prefix ~/.local && echo "🎉 Geruest installed locally!"
```

## 🧪 Quick Test

After installation, test that everything works:

**Create a test file:**
```cpp
// test.cpp
#include <iostream>
#include <Geruest.hpp>

int main() {
    std::cout << "✅ Geruest library works!" << std::endl;
    return 0;
}
```

**Compile and run:**
```bash
# Linux/macOS
g++ -std=c++17 test.cpp -lGeruest -lpthread -o test && ./test

# Windows (MinGW)
g++ -std=c++17 test.cpp -lGeruest -lws2_32 -o test.exe && ./test.exe
```

## 🛠️ Using in Your Project

### CMake Integration (Recommended)

Add this to your `CMakeLists.txt`:
```cmake
find_package(Geruest REQUIRED)
target_link_libraries(your_target Geruest::Geruest)
```

### Manual Compilation

```bash
# Linux/macOS
g++ -std=c++17 your_file.cpp -lGeruest -lpthread -o your_app

# Windows
g++ -std=c++17 your_file.cpp -lGeruest -lws2_32 -o your_app.exe
```

## 📚 Example Usage

```cpp
#include <Geruest.hpp>

int main() {
    Geruest::Server server;
    server.setPort(8080);
    server.setHostname("localhost");
    
    // Add a simple route
    server.addRoute("/hello", [](const Geruest::HTTPRequest& req) {
        Geruest::HTTPResponse response;
        response.setBody("Hello, World!");
        response.setStatusCode(200);
        return response;
    });
    
    server.start();
    return 0;
}
```

## 🔧 Custom Installation Options

### Install to Specific Directory

**Windows:**
```powershell
# Install to C:\MyLibs\Geruest
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release --prefix "C:\MyLibs\Geruest"
```

**Linux/macOS:**
```bash
# Install to ~/MyLibs/Geruest
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && cmake --install . --prefix ~/MyLibs/Geruest
```

### Debug Build

For development with debug symbols:
```bash
# Replace -DCMAKE_BUILD_TYPE=Release with -DCMAKE_BUILD_TYPE=Debug
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . && sudo cmake --install .
```

## 🚨 Troubleshooting

### Permission Errors
- **Windows**: Run PowerShell as Administrator
- **Linux/macOS**: Use `sudo` or install to user directory (`--prefix ~/.local`)

### Library Not Found
Set the installation path in your environment:
```bash
export CMAKE_PREFIX_PATH="/path/to/geruest/install:$CMAKE_PREFIX_PATH"
```

### MinGW Linking Issues
Switch to MSVC build on Windows or ensure MinGW is properly installed.

## 📖 Need More Details?

For detailed instructions and troubleshooting, see:
- [Advanced Build Instructions](BUILD_INSTRUCTIONS.md)
- [Installation Details](INSTALLATION_INSTRUCTIONS.md)
- [Platform-Specific Guides](WIN_BUILD_INSTRUCTIONS.md) | [Linux Guide](LINUX_BUILD_INSTRUCTIONS.md)

---

**Quick Reference:**
- 🖥️ Windows: Use PowerShell commands above
- 🐧 Linux/macOS: Use bash commands above
- 🧪 Test: Compile with `-lGeruest -lpthread` (Linux) or `-lGeruest -lws2_32` (Windows)
- 📚 Use: `find_package(Geruest REQUIRED)` in CMake
