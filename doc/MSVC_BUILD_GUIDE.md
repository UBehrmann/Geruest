# Building Geruest with MSVC

This guide provides step-by-step instructions for building the Geruest library with Microsoft Visual C++ (MSVC) compiler on Windows.

## Prerequisites

1. **Visual Studio 2017 or later** (Community, Professional, or Enterprise)
   - Or **Visual Studio Build Tools** for a lighter installation
   - Must include "Desktop development with C++" workload
   - This provides the MSVC compiler, Windows SDK, and MSBuild

2. **CMake 3.10 or later**
   - Download from: https://cmake.org/download/
   - Choose "Add CMake to the system PATH" during installation

## Installation Guide

### Option 1: Visual Studio Community (Recommended)

1. Download from: https://visualstudio.microsoft.com/vs/community/
2. Run the installer
3. Select the **"Desktop development with C++"** workload
4. Install

### Option 2: Visual Studio Build Tools (Lighter)

1. Download from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
2. Run the installer
3. Select **"C++ build tools"**
4. Install

### Option 3: Using Package Manager

If you have **Chocolatey** installed:

```powershell
# Install Visual Studio Build Tools
choco install visualstudio2022buildtools -y
choco install visualstudio2022-workload-vctools -y

# Install CMake
choco install cmake -y
```

If you have **Winget** installed:

```powershell
# Install Visual Studio Build Tools
winget install Microsoft.VisualStudio.2022.BuildTools

# Install CMake
winget install Kitware.CMake
```

## Build Instructions

### Step 1: Configure Library Build

```powershell
# Create build directory
Remove-Item -Recurse -Force build-msvc-lib -ErrorAction SilentlyContinue
mkdir build-msvc-lib
cd build-msvc-lib

# Configure with CMake
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64 -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
```

### Step 2: Build Library

```powershell
cmake --build . --config Release
```

### Step 3: Configure Example Build

```powershell
# Navigate to example directory
cd ..\exemple

# Create build directory
Remove-Item -Recurse -Force build-msvc -ErrorAction SilentlyContinue
mkdir build-msvc
cd build-msvc

# Configure with CMake
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64 -A x64 -DCMAKE_BUILD_TYPE=Release
```

### Step 4: Build Example

```powershell
cmake --build . --config Release
```

## Output Files

After successful build, you'll find:

- **Library**: `build-msvc-lib/Release/Geruest.lib`
- **Library (renamed)**: `build-msvc-lib/Geruest-msvc.lib`
- **Example**: `exemple/build-msvc/Release/exemple.exe`

## CMake Configuration Options

The project supports the following CMake options for MSVC builds:

- `-DCMAKE_GENERATOR_PLATFORM=x64`: Build for 64-bit architecture
- `-A x64`: Architecture specification (alternative syntax)
- `-DCMAKE_BUILD_TYPE=Release`: Build in release mode for optimal performance
- `-DBUILD_SHARED_LIBS=OFF`: Build static library (default)

## Troubleshooting

### Error: "cmake: command not found"

- CMake is not installed or not in PATH
- Install CMake and restart your terminal/PowerShell

### Error: "Visual Studio not found"

- Visual Studio or Build Tools not installed
- Install Visual Studio with C++ workload

### Error: "MSVC compiler not found"

- C++ tools not installed with Visual Studio
- Reinstall Visual Studio and ensure "Desktop development with C++" workload is selected

### Error: "Windows SDK not found"

- Windows SDK not installed
- Install Windows SDK through Visual Studio Installer

## Comparing with MinGW

| Feature | MSVC | MinGW |
|---------|------|-------|
| Compiler | Microsoft Visual C++ | GCC |
| Library format | `.lib` | `.a` |
| Runtime | Visual C++ Runtime | GCC Runtime |
| Debugging | Excellent VS integration | Good with GDB |
| Performance | Optimized for Windows | Cross-platform |
| Compatibility | Best Windows compatibility | More portable |

## Integration with Visual Studio

After building, you can integrate the library into Visual Studio projects:

1. Add `build-msvc-lib/Geruest-msvc.lib` to your project's library dependencies
2. Add `src/` directory to your include paths
3. Link against `ws2_32.lib` for Windows socket support

## Next Steps

- Read the main [README.md](../README.md) for library usage
- Check [WIN_BUILD_INSTRUCTIONS.md](WIN_BUILD_INSTRUCTIONS.md) for additional build options
- Explore the [exemple/](../exemple/) directory for usage examples
