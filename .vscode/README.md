# VS Code Configuration for Geruest Library

This VS Code configuration provides tasks for building the Geruest library and running the example application.

## Prerequisites

Make sure you have the following installed:
- CMake (version 3.10 or higher)
- Either:
  - **MSVC** (Visual Studio Build Tools or Visual Studio)
  - **MinGW** (for GCC compilation on Windows)
- VS Code Extensions:
  - C/C++ Extension Pack
  - CMake Tools (recommended)

## Available Tasks

### Library Build Tasks
- **Full Build (MSVC)** - Complete build and install of the library using MSVC
- **Full Build (MinGW)** - Complete build and install of the library using MinGW
- **Build Library (MSVC)** - Build only (default build task)
- **Build Library (MinGW)** - Build using MinGW
- **Clean Build Directory** - Clean the build directory

### Example Tasks
- **Build and Run Example (MSVC)** - Build and run the example server with MSVC
- **Build and Run Example (MinGW)** - Build and run the example server with MinGW
- **Run Example** - Run the example server (MSVC build)
- **Run Example (MinGW)** - Run the example server (MinGW build)

## Quick Start

### Method 1: Using Tasks
1. Open VS Code in the Geruest directory
2. Press `Ctrl+Shift+P` and type "Tasks: Run Task"
3. Choose one of the following:
   - **Full Build (MSVC)** - to build the library
   - **Build and Run Example (MSVC)** - to build and run the example

### Method 2: Using Build Shortcut
1. Press `Ctrl+Shift+P` and type "Tasks: Run Build Task"
2. This will run the default build task (Build Library MSVC)

### Method 3: Using Debug Configuration
1. Open `exemple/exemple.cpp`
2. Press `F5` to start debugging
3. Choose either:
   - **Debug Example (MSVC)** - for MSVC builds
   - **Debug Example (MinGW)** - for MinGW builds

## Example Server

The example server will:
- Start on `localhost:80`
- Serve files from the `website` folder
- Display "Starting Geruest server..." in the terminal

To stop the server, press `Ctrl+C` in the terminal.

## Compiler Configuration

The configuration supports both MSVC and MinGW:
- **MSVC**: Uses Visual Studio compiler (recommended for Windows)
- **MinGW**: Uses GCC compiler (alternative option)

You can switch between compilers by choosing the appropriate tasks.

## Troubleshooting

- If CMake is not found, ensure it's in your PATH
- If MSVC is not found, install Visual Studio Build Tools
- If MinGW is not found, install MinGW-w64
- The example server requires administrator privileges to bind to port 80. You may want to change the port in `exemple.cpp` if needed.
