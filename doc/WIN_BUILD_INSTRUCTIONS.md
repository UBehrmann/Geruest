# Geruest Library Build Instructions (Windows)

This project provides multiple CMake configurations for different build scenarios on Windows with support for both MinGW and MSVC compilers.

## Prerequisites

### For MinGW builds:
- MinGW-w64 (GCC 8.0 or later recommended)
- CMake 3.10 or later
- Git (optional, for cloning)

### For MSVC builds:
- Visual Studio 2017 or later (or Visual Studio Build Tools) with C++ support
- CMake 3.10 or later
- Git (optional, for cloning)

## Quick Start - MSVC Build (Recommended)

If you have Visual Studio installed and want to build with MSVC, use the automated scripts:

```powershell
# First-time setup (installs required tools if missing)
.\setup_scripts\setup_msvc.ps1

# Build the project
.\setup_scripts\build_msvc.ps1
```

Or using Command Prompt:
```cmd
setup_scripts\build_msvc.bat
```

This will build:
- **Library**: `build-msvc-lib/Geruest-msvc.lib`
- **Example**: `exemple/build-msvc/Release/exemple.exe`

## Manual MSVC Build

### 1. Building the Library with MSVC

```powershell
Remove-Item -Recurse -Force build-msvc-lib -ErrorAction SilentlyContinue
mkdir build-msvc-lib
cd build-msvc-lib
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64 -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

### 2. Building the Example with MSVC

```powershell
cd exemple
Remove-Item -Recurse -Force build-msvc -ErrorAction SilentlyContinue
mkdir build-msvc
cd build-msvc
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## MinGW Build (Alternative)

### 1. Building the Library with MinGW

```powershell
Remove-Item -Recurse -Force build-lib -ErrorAction SilentlyContinue
mkdir build-lib
cd build-lib
cmake -G "MinGW Makefiles" ..
cmake --build .
```

This builds only the Geruest static library (`lib/libGeruest.a`) with installation rules.

### 2. Building the Example with MinGW (Standalone)

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

- **MSVC Library**: `build-msvc-lib/Geruest-msvc.lib`
- **MSVC Example**: `exemple/build-msvc/Release/exemple.exe`
- **MinGW Library**: `build-lib/lib/libGeruest.a`
- **MinGW Example**: `exemple/build/exemple.exe`

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
