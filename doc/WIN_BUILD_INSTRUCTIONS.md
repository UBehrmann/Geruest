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

## Manual MinGW-w64 Installation/Update

If you need to install or update MinGW-w64 manually (without package managers):

### Step 1: Download Latest MinGW-w64
1. Visit https://winlibs.com/ (recommended) or https://www.mingw-w64.org/downloads/
2. Download the latest release (e.g., `winlibs-x86_64-posix-seh-gcc-13.2.0-mingw-w64-11.0.1-r5.7z`)
3. Choose the UCRT runtime version for better Windows compatibility

### Step 2: Install/Update
1. Extract the downloaded archive to `C:\mingw64` (or your preferred location)
2. If updating an existing installation, backup your old MinGW folder first
3. Update your PATH environment variable to point to the new `C:\mingw64\bin`

### Step 3: Update PATH
```powershell
# Temporary (current session only)
$env:PATH = "C:\mingw64\bin;" + $env:PATH

# Permanent (requires administrator)
[Environment]::SetEnvironmentVariable("PATH", "C:\mingw64\bin;" + [Environment]::GetEnvironmentVariable("PATH", "Machine"), "Machine")
```

### Step 4: Verify Installation
```powershell
gcc --version
g++ --version
```

## Troubleshooting

### MinGW Not Found
If cmake cannot find MinGW, make sure it's in your PATH:
```powershell
$env:PATH = "C:\mingw64\bin;" + $env:PATH
```

### Old MinGW Version
If you have an old MinGW installation (like GCC 6.3.0), you should update to at least GCC 8.0 or later:
```powershell
# Check current version
gcc --version

# If version is older than 8.0, follow the manual installation steps above
```

### DLL Issues
If you get DLL missing errors, ensure you're using the statically linked build or copy the required DLLs from your MinGW installation.

## Usage Summary

- **For library development**: Use the main CMakeLists.txt in the root directory
- **For example/testing**: Use the CMakeLists.txt in the `exemple/` directory
- Both configurations support static linking and produce self-contained executables
