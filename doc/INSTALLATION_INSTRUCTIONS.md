# Geruest Library Installation Instructions

This guide provides step-by-step instructions for installing the Geruest library on your system or for use in other projects.

## 🚀 Quick Installation

**Want to install immediately? Use these one-line commands:**

### Windows (PowerShell)
```powershell
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release
```

### Linux/macOS (Bash)
```bash
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && sudo cmake --install .
```

**For more installation options, see [BUILD_SCRIPTS.md](BUILD_SCRIPTS.md) or [QUICK_START.md](QUICK_START.md)**.

---

## Detailed Installation Guide

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation Methods](#installation-methods)
3. [System Installation](#system-installation)
4. [Local Installation](#local-installation)
5. [Using the Installed Library](#using-the-installed-library)
6. [Verification](#verification)
7. [Troubleshooting](#troubleshooting)

## Prerequisites

Before installing Geruest, ensure you have:

- **CMake 3.10 or higher**
- **C++17 compatible compiler**:
  - Windows: Visual Studio 2019+ or MinGW-w64
  - Linux: GCC 7+ or Clang 5+
- **Git** (for cloning the repository)

## Installation Methods

### Method 1: System Installation (Recommended)

This method installs the library system-wide, making it available to all projects.

#### Windows (PowerShell)

```powershell
# Clone the repository
git clone https://github.com/UBehrmann/Geruest.git
cd Geruest

# Create and configure build directory
mkdir build-release
cd build-release

# Configure with CMake (choose one):
# Option A: MSVC (recommended for Windows)
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release

# Option B: MinGW
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build the library
cmake --build . --config Release

# Install system-wide (requires administrator privileges)
cmake --install . --config Release
```

#### Linux (Bash)

```bash
# Clone the repository
git clone https://github.com/UBehrmann/Geruest.git
cd Geruest

# Create and configure build directory
mkdir build-release
cd build-release

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Install system-wide (requires sudo)
sudo cmake --install .
```

### Method 2: Local Installation

This method installs the library to a specific directory, useful for project-specific installations.

#### Windows (PowerShell)

```powershell
# Clone and build (same as above)
git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
mkdir build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Install to custom location
cmake --install . --config Release --prefix "C:\Libraries\Geruest"
```

#### Linux (Bash)

```bash
# Clone and build (same as above)
git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
mkdir build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Install to custom location
cmake --install . --prefix "$HOME/Libraries/Geruest"
```

### Method 3: Debug Installation

For development purposes, you may want to install both Release and Debug versions:

```bash
# Build and install Release version
mkdir build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install . --prefix ../install

# Build and install Debug version
cd ..
mkdir build-debug
cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
cmake --install . --prefix ../install
```

## Installation Layout

After installation, the library will be organized as follows:

```
Installation Directory/
├── lib/
│   ├── libGeruest.a                    # Static library
│   └── cmake/
│       └── Geruest/
│           ├── GeruestConfig.cmake         # Main config file
│           ├── GeruestConfigVersion.cmake  # Version info
│           ├── GeruestTargets.cmake        # Target definitions
│           ├── GeruestTargets-release.cmake # Release config
│           └── GeruestTargets-debug.cmake   # Debug config (if installed)
└── include/
    ├── Geruest.hpp                     # Main header
    ├── builders/                       # Builder classes
    │   ├── ContentBuilder.hpp
    │   ├── CSSBuilder.hpp
    │   ├── HTMLBuilder.hpp
    │   └── JSBuilder.hpp
    ├── data/                          # Data structures
    │   ├── HTTPRequest.hpp
    │   ├── HTTPResponse.hpp
    │   └── ServerData.hpp
    ├── FileManagement/                # File utilities
    │   └── FileManagement.hpp
    ├── handler/                       # Request handlers
    │   └── Handler.hpp
    └── parser/                        # Parsers
        └── JSONParser.hpp
```

## Using the Installed Library

### CMake Integration

After installation, you can use the library in your CMake projects:

```cmake
# In your CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(MyProject)

# Find the installed library
find_package(Geruest REQUIRED)

# Create your executable
add_executable(myapp main.cpp)

# Link against Geruest
target_link_libraries(myapp Geruest::Geruest)
```

### Manual Integration

If not using CMake, you can manually link against the library:

#### Compilation flags:
- **Include path**: Add `-I/path/to/install/include`
- **Library path**: Add `-L/path/to/install/lib`
- **Link library**: Add `-lGeruest`
- **Required system libraries**:
  - Windows: `-lws2_32` (for networking)
  - Linux: `-lpthread` (for threading)

#### Example compilation:
```bash
# Linux
g++ -std=c++17 main.cpp -I/usr/local/include -L/usr/local/lib -lGeruest -lpthread

# Windows (MinGW)
g++ -std=c++17 main.cpp -I"C:\Program Files\Geruest\include" -L"C:\Program Files\Geruest\lib" -lGeruest -lws2_32
```

## Verification

### Test the Installation

Create a simple test program to verify the installation:

```cpp
// test_geruest.cpp
#include <Geruest.hpp>
#include <iostream>

int main() {
    // Test basic functionality
    Geruest::Server server;
    std::cout << "Geruest library loaded successfully!" << std::endl;
    std::cout << "Server object created." << std::endl;
    return 0;
}
```

Compile and run:
```bash
# Using CMake find_package (recommended)
mkdir test && cd test
echo 'cmake_minimum_required(VERSION 3.10)
project(test)
find_package(Geruest REQUIRED)
add_executable(test_geruest test_geruest.cpp)
target_link_libraries(test_geruest Geruest::Geruest)' > CMakeLists.txt

cmake .
cmake --build .
./test_geruest  # Linux
# or
./test_geruest.exe  # Windows
```

### Check Installation Files

Verify that all files are installed correctly:

```bash
# Check library file
ls -la /usr/local/lib/libGeruest.a  # Linux
dir "C:\Program Files\Geruest\lib\libGeruest.a"  # Windows

# Check CMake config files
ls -la /usr/local/lib/cmake/Geruest/  # Linux
dir "C:\Program Files\Geruest\lib\cmake\Geruest\"  # Windows

# Check headers
ls -la /usr/local/include/Geruest.hpp  # Linux
dir "C:\Program Files\Geruest\include\Geruest.hpp"  # Windows
```

## Troubleshooting

### Common Issues

1. **Permission Denied During Installation**
   - **Windows**: Run PowerShell as Administrator
   - **Linux**: Use `sudo` for system-wide installation

2. **Library Not Found**
   - Ensure the installation directory is in your system's library path
   - For custom installations, set `CMAKE_PREFIX_PATH` environment variable

3. **Missing Dependencies**
   - **Windows**: Ensure `ws2_32.dll` is available (usually system-provided)
   - **Linux**: Install pthread library (`sudo apt-get install libpthread-stubs0-dev`)

4. **CMake Cannot Find Package**
   ```bash
   # Set the path where Geruest was installed
   export CMAKE_PREFIX_PATH="/path/to/geruest/install:$CMAKE_PREFIX_PATH"
   
   # Or use find_package with PATHS
   find_package(Geruest REQUIRED PATHS "/path/to/geruest/install")
   ```

5. **Linking Errors**
   - Ensure you're linking against the correct architecture (x64 vs x86)
   - Check that the C++ standard matches (both project and library should use C++17)

### Getting Help

If you encounter issues:

1. Check the [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for build-specific guidance
2. Review platform-specific instructions:
   - [WIN_BUILD_INSTRUCTIONS.md](WIN_BUILD_INSTRUCTIONS.md)
   - [LINUX_BUILD_INSTRUCTIONS.md](LINUX_BUILD_INSTRUCTIONS.md)
3. Open an issue on the GitHub repository with:
   - Your operating system and version
   - CMake version (`cmake --version`)
   - Compiler version
   - Complete error messages
   - Steps to reproduce the issue

## Advanced Usage

### Multi-Configuration Support

The installation supports both Release and Debug configurations. CMake will automatically select the appropriate version based on your project's build type:

```cmake
# This will use GeruestTargets-release.cmake for Release builds
# and GeruestTargets-debug.cmake for Debug builds
find_package(Geruest REQUIRED)
target_link_libraries(myapp Geruest::Geruest)
```

### Custom Installation Locations

For projects that need to bundle the library:

```bash
# Install to project-specific location
cmake --install . --prefix "./third_party/geruest"

# In your CMakeLists.txt
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/geruest")
find_package(Geruest REQUIRED)
```

This approach is useful for:
- Vendoring dependencies
- Creating portable applications
- Avoiding system-wide installations
