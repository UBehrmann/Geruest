# Geruest Framework - AI Coding Assistant Instructions

## Project Overview
Geruest is a lightweight C++17 web framework for building HTTP servers. Key architectural components:
- **Core Server**: `Geruest` class handles socket management, Windows/Linux compatibility
- **Request Pipeline**: `Handler` → `HTTPRequest` → Route matching → `HTTPResponse`
- **Route System**: Supports exact paths and wildcard patterns (`/api/*`, `/users/*/profile`)
- **Content Building**: Template system with `{component}` and `[translation]` substitution
- **Static Serving**: Automatic file serving from configurable root directory

## Critical Architecture Patterns

### Cross-Platform Socket Handling
Windows (`_WIN32`) uses `SOCKET`/`WSA*` APIs, Linux uses `int` file descriptors. All socket code follows this pattern:
```cpp
#ifdef _WIN32
    SOCKET server_fd = INVALID_SOCKET;
    // WSAStartup/WSACleanup required
#else
    int server_fd = -1;
    // Standard POSIX sockets
#endif
```

### Route Handler Signature
All routes use this exact function signature:
```cpp
using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;
server->addRoute("/path", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody("{}");
    return response;
});
```

### Wildcard Route Precedence
- Exact routes in `_routes` map (O(1) lookup)
- Wildcard routes in `_wildcardRoutes` map (O(n) lookup)  
- `findMatchingRoute()` tries exact match first, then wildcards
- Supports `*` patterns: `/api/*`, `/users/*/profile`, `/downloads/*.zip`

## Build System Specifics

### Multi-Compiler CMake Strategy
Project supports three build configurations with specific flags:
- **MSVC**: `-A x64 -DCMAKE_BUILD_TYPE=Release`
- **MinGW**: `-G "MinGW Makefiles"` with static linking flags
- **Linux**: Standard Unix Makefiles

### Task Dependencies
VS Code tasks have specific dependency chains:
1. `Create Build Directory` → `Configure CMake` → `Build Library` → `Install Library`
2. Example builds require library installation first
3. Use `Full Build (MSVC/MinGW)` for complete workflow

### Library Installation
CMake generates `GeruestConfig.cmake` for find_package() consumption. Install creates:
```
lib/
├── libGeruest.a (.lib on MSVC)
└── cmake/Geruest/
    ├── GeruestTargets.cmake  
    └── GeruestConfig.cmake
include/
└── [all .hpp files]
```

## Code Organization Principles

### Namespace Convention
All framework code in `geruest` namespace. User code uses `using namespace geruest;`.

### File Structure Pattern
- Headers in `src/` mirror class hierarchy
- Cross-platform code uses `#ifdef _WIN32` blocks
- Private methods grouped at bottom of classes
- Consistent header guards: `GERUEST_CLASSNAME_HPP`

### Error Handling Pattern
Consistent logging through `sendToLogger*()` methods:
- `sendToLogger()` - general info
- `sendToLoggerError()` - errors  
- `sendToLoggerAPI()` - API requests
- `sendToLoggerPages()` - page requests

### Resource Management
RAII pattern throughout:
- Sockets closed in destructors
- WSACleanup() called on Windows
- Buffer management with BUFFER_SIZE constant (8192)

## Development Workflow

### Build Commands
```powershell
# Full build (Windows MSVC)
mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release

# Full build (Windows MinGW) 
mkdir build; cd build; cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release; cmake --build .; cmake --install .
```

### Example Testing
Example in `/exemple` demonstrates all patterns. Run with:
- Task: "Build and Run Example (MSVC/MinGW)"
- Serves on port 80 with `/website` folder
- Signal handlers for graceful shutdown (Ctrl+C)

### Content Builder System
Template files support:
- `{component_name}` - includes from `/components/`
- `[translation_key]` - from `/assets/translations/`
- File maps in `/files_maps/` for CSS/JS bundling
- Comments removed by default (`setRemoveComments(false)` to keep)

## Integration Points

### External Dependencies
- **Threading**: `std::thread` for request handling
- **Filesystem**: `std::filesystem` for path resolution  
- **Network**: Platform-specific socket libraries
- **JSON**: Custom `JSONParser` class, not external library

### Website Structure Convention
Expected folder structure for `addRoot()`:
```
/assets/{css,js,images,translations,JSONs}/
/components/{header.html,footer.html}
/html/index.html
/configs/restrictions.json
/files_maps/{css_file_map.json,js_file_map.json}
```

## Critical Implementation Notes

### Memory Management
- Manual socket cleanup required (destructor handles this)  
- String copying minimal - use `std::move()` for RouteHandlers
- Buffer reuse with fixed BUFFER_SIZE allocation

### Thread Safety
- Each client connection handled in separate thread
- ServerData shared read-only after initialization
- No thread-local storage - stateless request handling

### Performance Considerations  
- Route matching optimized: exact O(1), wildcard O(n)
- Static file serving bypasses route system
- Request parsing happens once per connection
- Keep-alive not implemented - connection per request

When modifying this codebase, maintain cross-platform compatibility, follow the established error logging patterns, and test with both MSVC and MinGW builds.