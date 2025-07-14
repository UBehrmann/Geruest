# Geruest Library Build Instructions

This project provides multiple CMake configurations for different build scenarios.

## Platform-Specific Instructions

For detailed build instructions specific to your platform, please see:

- **Windows**: [WIN_BUILD_INSTRUCTIONS.md](WIN_BUILD_INSTRUCTIONS.md)
- **Linux**: [LINUX_BUILD_INSTRUCTIONS.md](LINUX_BUILD_INSTRUCTIONS.md)

## Quick Start

### Windows (PowerShell)
```powershell
# Build library
Remove-Item -Recurse -Force build-lib -ErrorAction SilentlyContinue
mkdir build-lib
cd build-lib
cmake -G "MinGW Makefiles" ..
cmake --build .

# Build example
cd ../exemple
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

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
