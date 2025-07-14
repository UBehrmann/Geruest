# GitHub Actions Workflow Test Guide

## Updated Workflow Features

The `.github/workflows/build.yml` has been updated to:

1. **Linux Build**: Uses `ubuntu-latest` with standard CMake
2. **Windows Build**: Uses `windows-latest` with MinGW-w64 via MSYS2
3. **Both produce**: Static library files (`libGeruest.a`)

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
   - Windows: `libGeruest.a` from MinGW build
   - Both attached to the GitHub release

### Local Testing Commands:

**Linux (or WSL):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

**Windows with MinGW:**
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

## Key Changes Made

1. **Separated build jobs**: Linux and Windows now run independently
2. **Added MinGW setup**: Windows uses MSYS2 to install MinGW-w64
3. **Fixed shell context**: Windows build uses `msys2 {0}` shell
4. **Corrected output paths**: Both builds output to `build/lib/libGeruest.a`

## Troubleshooting

If the workflow fails:
1. Check the Actions tab for detailed logs
2. Verify that the CMakeLists.txt builds locally with MinGW
3. Ensure all source files have proper includes (like `<algorithm>` for std::min)
