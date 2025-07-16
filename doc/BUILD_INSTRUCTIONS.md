# Geruest Library Build Instructions

This document provides comprehensive instructions for building the Geruest library. **For quick copy-paste commands, see [BUILD_SCRIPTS.md](BUILD_SCRIPTS.md)**.

## 🚀 Quick Start - Copy & Paste Commands

**Want to get started immediately? Use these one-line commands:**

### Windows (PowerShell)
```powershell
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release
```

### Linux/macOS (Bash)
```bash
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && sudo cmake --install .
```

**For more build options and scripts, see [BUILD_SCRIPTS.md](BUILD_SCRIPTS.md)**.

---

## Detailed Build Process

This document provides instructions for building the Geruest library for development purposes. If you want to install the library for use in other projects, see [INSTALLATION_INSTRUCTIONS.md](INSTALLATION_INSTRUCTIONS.md).

This project provides multiple CMake configurations for different build scenarios.

## Platform-Specific Instructions

For detailed build instructions specific to your platform, please see:

- **Windows**: [WIN_BUILD_INSTRUCTIONS.md](WIN_BUILD_INSTRUCTIONS.md)
- **Linux**: [LINUX_BUILD_INSTRUCTIONS.md](LINUX_BUILD_INSTRUCTIONS.md)

## Quick Start

### Windows (PowerShell) - MSVC Recommended
```powershell
# MSVC Build (Recommended for Windows)
cd exemple
Remove-Item -Recurse -Force build-msvc -ErrorAction SilentlyContinue
mkdir build-msvc
cd build-msvc
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Windows (PowerShell) - MinGW Alternative
```powershell
# Build library
Remove-Item -Recurse -Force build-lib -ErrorAction SilentlyContinue
mkdir build-lib
cd build-lib
cmake -G "MinGW Makefiles" ..
cmake --build . --config Release
cmake --install . --config Release

# Build example
cd ../exemple
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

**Note**: If you encounter MinGW linking errors (missing crt2.o, libmingw32, etc.), use the MSVC build instead.

### Linux (Bash)
```bash
# Build library
rm -rf build-lib
mkdir build-lib
cd build-lib
cmake -G "Unix Makefiles" ..
cmake --build .

# Build example
cd ../exemple
rm -rf build
mkdir build
cd build
cmake -G "Unix Makefiles" ..
cmake --build .
```

## Output Files

- **Library**: `build-lib/lib/libGeruest.a`
- **Example**: 
  - Windows: `exemple/build/exemple.exe`
  - Linux: `exemple/build/exemple`

## Key Features

- **Static linking on Windows**: No DLL dependencies required
- **Dynamic linking on Linux**: Uses system libraries
- **C++17 support**: Modern C++ features enabled
- **Cross-platform**: Works on Windows (MinGW) and Linux (GCC)

## Project Structure

```
Geruest/
├── CMakeLists.txt              # Library build configuration
├── exemple/
│   ├── CMakeLists.txt          # Example build configuration
│   └── exemple.cpp             # Example source code
├── src/                        # Library source code
├── WIN_BUILD_INSTRUCTIONS.md   # Windows-specific instructions
├── LINUX_BUILD_INSTRUCTIONS.md # Linux-specific instructions
└── BUILD_INSTRUCTIONS.md       # This file
```
