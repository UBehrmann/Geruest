# Geruest Library - Build & Install Scripts

This page provides simple copy-paste scripts to build and install the Geruest library. Choose the appropriate script for your platform and use case.

## 🚀 One-Command Installation

### Windows (PowerShell) - Recommended

**System-wide installation (requires admin privileges):**
```powershell
# Complete build and install
if (-not (Test-Path "Geruest")) { git clone https://github.com/UBehrmann/Geruest.git }
cd Geruest
if (Test-Path "build") { Remove-Item -Recurse -Force build }
mkdir build
cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --config Release
Write-Host "✅ Geruest installed successfully!" -ForegroundColor Green
```

**Local installation (no admin needed):**
```powershell
# Install to current user directory
if (-not (Test-Path "Geruest")) { git clone https://github.com/UBehrmann/Geruest.git }
cd Geruest
if (Test-Path "build") { Remove-Item -Recurse -Force build }
mkdir build
cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --config Release --prefix "$env:LOCALAPPDATA\Geruest"
Write-Host "✅ Geruest installed to $env:LOCALAPPDATA\Geruest" -ForegroundColor Green
```

### Linux/macOS (Bash)

**System-wide installation:**
```bash
# Complete build and install
[ ! -d "Geruest" ] && git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
sudo cmake --install .
echo "✅ Geruest installed successfully!"
```

**Local installation (no sudo needed):**
```bash
# Install to user directory
[ ! -d "Geruest" ] && git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install . --prefix ~/.local
echo "✅ Geruest installed to ~/.local"
echo "Add ~/.local/lib to your library path if needed"
```

## 🛠️ Development Builds

### Debug Build with Symbols

**Windows:**
```powershell
if (-not (Test-Path "Geruest")) { git clone https://github.com/UBehrmann/Geruest.git }
cd Geruest
if (Test-Path "build-debug") { Remove-Item -Recurse -Force build-debug }
mkdir build-debug
cd build-debug
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
cmake --install . --config Debug --prefix "debug-install"
Write-Host "✅ Debug build ready in debug-install/" -ForegroundColor Green
```

**Linux/macOS:**
```bash
[ ! -d "Geruest" ] && git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
rm -rf build-debug
mkdir build-debug
cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
cmake --install . --prefix debug-install
echo "✅ Debug build ready in debug-install/"
```

### Build Example Application

**Windows:**
```powershell
cd Geruest/exemple
if (Test-Path "build") { Remove-Item -Recurse -Force build }
mkdir build
cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
Write-Host "✅ Example built: Release/exemple.exe" -ForegroundColor Green
```

**Linux/macOS:**
```bash
cd Geruest/exemple
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
echo "✅ Example built: ./exemple"
```

## 🧪 Quick Test After Installation

**Test script (save as test.cpp):**
```cpp
#include <iostream>
#include <Geruest.hpp>

int main() {
    std::cout << "Testing Geruest library..." << std::endl;
    
    // Basic test - create server object
    try {
        Geruest::Server server;
        std::cout << "✅ Server object created successfully!" << std::endl;
        std::cout << "✅ Geruest library is working!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

**Compile and run test:**

**Windows:**
```powershell
# Using CMake (recommended)
echo 'cmake_minimum_required(VERSION 3.10)
project(test)
set(CMAKE_CXX_STANDARD 17)
find_package(Geruest REQUIRED)
add_executable(test test.cpp)
target_link_libraries(test Geruest::Geruest)' > CMakeLists.txt
cmake . -A x64
cmake --build . --config Release
.\Release\test.exe
```

**Linux/macOS:**
```bash
# Using CMake (recommended)
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.10)
project(test)
set(CMAKE_CXX_STANDARD 17)
find_package(Geruest REQUIRED)
add_executable(test test.cpp)
target_link_libraries(test Geruest::Geruest)
EOF
cmake .
cmake --build .
./test
```

## 📦 Custom Installation Paths

### Install to Specific Directory

**Windows - Custom Path:**
```powershell
$INSTALL_PATH = "C:\MyLibs\Geruest"
if (-not (Test-Path "Geruest")) { git clone https://github.com/UBehrmann/Geruest.git }
cd Geruest
if (Test-Path "build") { Remove-Item -Recurse -Force build }
mkdir build
cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --config Release --prefix $INSTALL_PATH
Write-Host "✅ Geruest installed to $INSTALL_PATH" -ForegroundColor Green
Write-Host "Set CMAKE_PREFIX_PATH=$INSTALL_PATH when using" -ForegroundColor Yellow
```

**Linux/macOS - Custom Path:**
```bash
INSTALL_PATH="$HOME/MyLibs/Geruest"
[ ! -d "Geruest" ] && git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install . --prefix "$INSTALL_PATH"
echo "✅ Geruest installed to $INSTALL_PATH"
echo "Set CMAKE_PREFIX_PATH=$INSTALL_PATH when using"
```

## 🔧 MinGW Build (Windows Alternative)

If you prefer MinGW over MSVC:

```powershell
if (-not (Test-Path "Geruest")) { git clone https://github.com/UBehrmann/Geruest.git }
cd Geruest
if (Test-Path "build") { Remove-Item -Recurse -Force build }
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install .
Write-Host "✅ Geruest built with MinGW!" -ForegroundColor Green
```

## 🚨 Troubleshooting

### Fix Common Issues

**Permission denied (Windows):**
```powershell
# Run PowerShell as Administrator, then run install script
```

**Permission denied (Linux/macOS):**
```bash
# Use local install instead of system-wide
cmake --install . --prefix ~/.local
```

**Library not found:**
```bash
# Set environment variable
export CMAKE_PREFIX_PATH="/path/to/geruest/install:$CMAKE_PREFIX_PATH"
```

**Check installation:**
```bash
# Verify files are installed
ls -la /usr/local/lib/libGeruest.a                    # Linux system
ls -la ~/.local/lib/libGeruest.a                      # Linux local
dir "C:\Program Files (x86)\Geruest\lib\libGeruest.a" # Windows system
```

---

## 📖 Next Steps

After successful installation:
1. ✅ **Test** - Run the test script above
2. 📚 **Learn** - Check the [example application](../exemple/exemple.cpp)
3. 🛠️ **Use** - Add `find_package(Geruest REQUIRED)` to your CMakeLists.txt
4. 🚀 **Build** - Start creating your web applications!

For more detailed information, see:
- [QUICK_START.md](QUICK_START.md) - Simple usage guide
- [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) - Detailed build info
- [INSTALLATION_INSTRUCTIONS.md](INSTALLATION_INSTRUCTIONS.md) - Advanced installation
