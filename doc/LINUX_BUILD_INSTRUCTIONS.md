# Geruest Library Build Instructions (Linux)

This project provides multiple CMake configurations for different build scenarios on Linux.

## Prerequisites

- GCC 8.0 or later (with C++17 support)
- CMake 3.10 or later
- pthread library (usually included)
- Git (optional, for cloning)

## 1. Building the Library

```bash
rm -rf build-lib
mkdir build-lib
cd build-lib
cmake -G "Unix Makefiles" ..
cmake --build .
```

This builds only the Geruest static library (`lib/libGeruest.a`) with installation rules.

## 2. Building the Example (Standalone)

```bash
cd exemple
rm -rf build
mkdir build
cd build
cmake -G "Unix Makefiles" ..
cmake --build .
```

This builds the example executable (`exemple`) with its own embedded copy of the library.

## Alternative: Using Make directly

```bash
# For library
mkdir build-lib && cd build-lib
cmake ..
make

# For example
cd exemple
mkdir build && cd build
cmake ..
make
```

## Output Files

- **Library**: `build-lib/lib/libGeruest.a`
- **Example**: `exemple/build/exemple`

## Dynamic vs Static Linking

On Linux, the build uses dynamic linking by default. The executable will depend on:
- `libpthread.so` (usually system-provided)
- `libstdc++.so` (usually system-provided)

## Installation

When using the library build, you can install headers and library:

```bash
cd build-lib
sudo cmake --install . --prefix /usr/local
```

This will install:
- Headers to `/usr/local/include/`
- Library to `/usr/local/lib/`

## Release Build

For optimized release builds:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

This enables compiler optimizations and strips debug symbols.

## Troubleshooting

### Missing pthread
If you get pthread linking errors:
```bash
sudo apt-get install libpthread-stubs0-dev  # Ubuntu/Debian
sudo yum install glibc-devel                # CentOS/RHEL
```

### Missing C++17 support
Ensure you have GCC 8.0 or later:
```bash
gcc --version
g++ --version
```

### Permission issues
If installation fails, make sure you have proper permissions or use `sudo`.

## Usage Summary

- **For library development**: Use the main CMakeLists.txt in the root directory
- **For example/testing**: Use the CMakeLists.txt in the `exemple/` directory
- Linux builds use dynamic linking by default
- Use Release build type for production deployments
