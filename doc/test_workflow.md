# GitHub Actions Workflow Test Guide

## Updated Workflow Features

The `.github/workflows/build.yml` has been updated to:

1. **Linux Build**: Uses `ubuntu-latest` with standard CMake
2. **Windows MinGW Build**: Uses `windows-latest` with MinGW-w64 via MSYS2
3. **Windows MSVC Build**: Uses `windows-latest` with Visual Studio
4. **Outputs**: 
   - Linux: `libGeruest.a` (static library)
   - Windows MinGW: `libGeruest-windows.a` (static library)
   - Windows MSVC: `Geruest-msvc.lib` (static library)

## Testing the Workflow

### Manual Testing Steps:

1. **Create a git tag** to trigger the workflow:
```bash
git tag v1.0.0
git push origin v1.0.0
```

2. **Check GitHub Actions** tab in your repository to see the build progress

3. **Expected outputs**:
   - Linux: `libGeruest.a` from Ubuntu build
   - Windows MinGW: `libGeruest-windows.a` from MinGW build
   - Windows MSVC: `Geruest-msvc.lib` from Visual Studio build
   - All attached to the GitHub release

### Local Testing Commands:

**Linux (or WSL):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

**Windows with MSVC (Visual Studio):**
```powershell
mkdir build && cd build
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64 -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

**Windows with MinGW:**
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

## Key Changes Made

1. **Separated build jobs**: Linux, Windows MinGW, and Windows MSVC now run independently
2. **Added MinGW setup**: Windows MinGW uses MSYS2 to install MinGW-w64
3. **Added MSVC support**: Windows MSVC uses Visual Studio compiler
4. **Fixed shell context**: Windows MinGW build uses `msys2 {0}` shell
5. **Corrected output paths**: 
   - Linux: `build/lib/libGeruest.a`
   - MinGW: `build/lib/libGeruest.a` → `build/lib/libGeruest-windows.a`
   - MSVC: `build/lib/Release/Geruest.lib` → `build/Geruest-msvc.lib`

## Troubleshooting

If the workflow fails:
1. Check the Actions tab for detailed logs
2. Verify that the CMakeLists.txt builds locally with MinGW
3. Ensure all source files have proper includes (like `<algorithm>` for std::min)
