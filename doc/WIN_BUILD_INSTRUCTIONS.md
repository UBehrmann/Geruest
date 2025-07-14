# Geruest Library Build Instructions (Windows)

This project provides multiple CMake configurations for different build scenarios on Windows.

## Prerequisites

- MinGW-w64 (GCC 8.0 or later recommended)
- CMake 3.10 or later
- Git (optional, for cloning)

## 1. Building the Library

```powershell
Remove-Item -Recurse -Force build-lib -ErrorAction SilentlyContinue
mkdir build-lib
cd build-lib
cmake -G "MinGW Makefiles" ..
cmake --build .
```

This builds only the Geruest static library (`lib/libGeruest.a`) with installation rules.

## 2. Building the Example (Standalone)

```powershell
cd exemple
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

This builds the example executable (`exemple.exe`) with its own embedded copy of the library.

## Alternative Commands (CMD)

If you prefer using Command Prompt instead of PowerShell:

### Building the Library
```cmd
rmdir /s /q build-lib 2>nul
mkdir build-lib
cd build-lib
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### Building the Example
```cmd
cd exemple
rmdir /s /q build 2>nul
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

## Output Files

- **Library**: `build-lib/lib/libGeruest.a`
- **Example**: `exemple/build/exemple.exe`

## Static Linking

All builds use static linking by default on Windows with MinGW, meaning the executables don't require additional DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, etc.).

## Installation

When using the library build, you can install headers and library:

```powershell
cd build-lib
cmake --install . --prefix C:\path\to\install
```

This will install:
- Headers to `C:\path\to\install\include\`
- Library to `C:\path\to\install\lib\`

## Troubleshooting

### MinGW Not Found
If cmake cannot find MinGW, make sure it's in your PATH:
```powershell
$env:PATH = "C:\mingw-w64\bin;" + $env:PATH
```

### DLL Issues
If you get DLL missing errors, ensure you're using the statically linked build or copy the required DLLs from your MinGW installation.

## Usage Summary

- **For library development**: Use the main CMakeLists.txt in the root directory
- **For example/testing**: Use the CMakeLists.txt in the `exemple/` directory
- Both configurations support static linking and produce self-contained executables
