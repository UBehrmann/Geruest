# Geruest Framework - AI Coding Assistant Instructions

## 🎯 Project Overview
Geruest is a modern, lightweight C++17 web framework designed for building high-performance HTTP servers with cross-platform compatibility. The framework emphasizes simplicity, performance, and ease of use.

### Core Architecture Components
- **🔧 Core Server**: `Geruest` class manages socket operations, threading, and cross-platform compatibility
- **🔄 Request Pipeline**: `Handler` → `HTTPRequest` → Route matching → `HTTPResponse` → Client response
- **🛣️ Route System**: Intelligent routing with exact paths and wildcard patterns (`/api/*`, `/users/*/profile`)
- **📝 Content Building**: Advanced template system with `{component}` inclusion and `[translation]` substitution
- **📁 Static Serving**: Optimized automatic file serving from configurable root directories
- **🔍 JSON Processing**: Custom `JSONParser` using string-based storage (no `std::any` dependencies)

## 🏗️ Critical Architecture Patterns

### Cross-Platform Socket Management
**Windows vs Linux Compatibility** - Essential for all network operations:
```cpp
#ifdef _WIN32
    SOCKET server_fd = INVALID_SOCKET;
    WSAData wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);  // Required initialization
    // Use closesocket(), WSACleanup() for cleanup
#else
    int server_fd = -1;
    // Use close() for cleanup, standard POSIX sockets
#endif
```

### Route Handler Pattern
**Consistent Function Signature** - All routes must follow this exact pattern:
```cpp
using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;

// Example route implementation
server->addRoute("/api/users", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setBody(R"({"users": []})");
    return response;
});
```

### Route Matching Hierarchy
**Performance-Optimized Lookup Strategy**:
1. **Exact Routes** (`_routes` map) - O(1) hash lookup for precise matches
2. **Wildcard Routes** (`_wildcardRoutes` map) - O(n) pattern matching for flexible routing
3. **Static Files** - Direct filesystem serving bypassing route system
4. **404 Fallback** - Default handler for unmatched requests

**Supported Wildcard Patterns**:
- `/api/*` - Matches any single path segment after `/api/`
- `/users/*/profile` - Matches `/users/{id}/profile` patterns
- `/downloads/*.{zip,pdf}` - File extension matching
- `/*` - Catch-all route (use sparingly)

### JSON Parser Architecture
**String-Based Storage System** - No `std::any` dependencies:
```cpp
// Storage: std::map<std::string, std::string>
parser.setInt("age", 25);        // Stores "25" as string
int age = parser.getInt("age");  // Converts back to int using std::stoi()

// Type conversion responsibility is on the user
parser.setFloat("price", 19.99);
float price = parser.getFloat("price");  // User must know correct type
```

## 🔨 Build System Mastery

### Multi-Compiler Strategy
**Three Primary Build Configurations**:

#### MSVC (Recommended for Windows)
```powershell
mkdir build && cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --config Release
```

#### MinGW (Alternative Windows)
```powershell
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install .
```

#### Linux/Unix
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
make install
```

### VS Code Task Dependencies
**Automated Build Pipeline**:
```
Clean Build → Create Build Directory → Configure CMake → Build Library → Install Library
                                                    ↓
                                            Example Projects (depends on install)
```

### Library Installation Structure
**Generated Installation Layout**:
```
install_prefix/
├── lib/
│   ├── libGeruest.a (Unix) / Geruest.lib (Windows)
│   └── cmake/Geruest/
│       ├── GeruestTargets.cmake
│       ├── GeruestTargets-release.cmake
│       └── GeruestConfig.cmake
├── include/
│   └── geruest/
│       ├── Geruest.hpp
│       ├── data/
│       ├── builders/
│       └── [all headers]
└── bin/ (if executables)
```

## 📁 Code Organization Standards

### Namespace Strategy
```cpp
namespace geruest {
    // All framework code lives here
    class Geruest { /* ... */ };
    class HTTPRequest { /* ... */ };
}

// User code patterns
using namespace geruest;  // In implementation files
// OR
geruest::Geruest server;  // In headers
```

### File Structure Convention
```
src/
├── Geruest.{cpp,hpp}              # Main server class
├── data/
│   ├── HTTPRequest.{cpp,hpp}      # Request parsing/handling
│   ├── HTTPResponse.{cpp,hpp}     # Response building
│   └── ServerData.hpp             # Shared server configuration
├── builders/
│   ├── ContentBuilder.{cpp,hpp}   # Template processing
│   ├── HTMLBuilder.{cpp,hpp}      # HTML-specific building
│   ├── CSSBuilder.{cpp,hpp}       # CSS bundling/processing
│   └── JSBuilder.{cpp,hpp}        # JavaScript bundling
├── handler/
│   └── Handler.{cpp,hpp}          # Request routing logic
├── parser/
│   └── JSONParser.{cpp,hpp}       # JSON processing
└── FileManagement/
    └── FileManagement.{cpp,hpp}   # File I/O utilities
```

### Header Guard Pattern
```cpp
#ifndef GERUEST_CLASSNAME_HPP
#define GERUEST_CLASSNAME_HPP
// ... content ...
#endif
```

### Error Handling & Logging
**Standardized Logging Interface**:
```cpp
// Use appropriate logger based on context
sendToLogger("Server started on port 8080");           // General info
sendToLoggerError("Failed to bind socket");            // Error conditions
sendToLoggerAPI("POST /api/users - 201 Created");      // API request logging
sendToLoggerPages("GET /index.html - 200 OK");         // Page request logging
```

## 🚀 Development Workflow

### Testing Strategy
```bash
# All Unit Tests (Recommended - runs all 174 tests)
cd src/unitTests/build && ctest --output-on-failure

# Individual Test Modules
./build/JSONParser_Tests        # 38 tests
./build/HTTPRequest_Tests       # 6 tests
./build/HTTPResponse_Tests      # 8 tests
./build/FileManagement_Tests    # 9 tests
./build/ContentBuilder_Tests    # 8 tests
./build/JSObfuscator_Tests      # 14 tests
./build/BasicAuth_Tests         # 34 tests
./build/ConfigLoader_Tests      # 56 tests
./build/AssetMerger_Tests       # 18 tests

# Example Application
# Serves on port 80 with graceful shutdown (Ctrl+C)
./exemple/build/exemple
```

### Content Builder System
**Advanced Template Processing**:

#### Component Inclusion
```html
<!-- In template files -->
{header}           <!-- Includes /components/header.html -->
{navigation}       <!-- Includes /components/navigation.html -->
{footer}           <!-- Includes /components/footer.html -->
```

#### Translation System
```html
<!-- Language-aware content -->
[welcome_message]  <!-- From /assets/translations/{lang}.json -->
[page_title]       <!-- Automatically localized -->
```

#### File Bundling
```json
// /files_maps/css_file_map.json
{
  "bundle_name": "main.css",
  "files": ["reset.css", "layout.css", "components.css"]
}

// /files_maps/js_file_map.json
{
  "bundle_name": "app.js", 
  "files": ["utils.js", "main.js", "components.js"]
}
```

#### Comment Handling
```cpp
ContentBuilder builder;
builder.setRemoveComments(false);  // Keep HTML/CSS comments
// Default: true (removes <!-- --> and /* */ comments)
```

## 🌐 Integration & Usage Patterns

### Expected Website Structure
**Standard Directory Layout for `addRoot()`**:
```
website_root/
├── assets/
│   ├── css/              # Stylesheets
│   ├── js/               # JavaScript files
│   ├── images/           # Static images
│   ├── translations/     # Language files (en.json, fr.json, etc.)
│   └── JSONs/           # Data files
├── components/
│   ├── header.html       # Reusable components
│   ├── footer.html
│   └── navigation.html
├── html/
│   ├── index.html        # Main pages
│   └── about.html
├── configs/
│   └── restrictions.json # Access control rules
└── files_maps/
    ├── css_file_map.json # CSS bundling configuration
    └── js_file_map.json  # JS bundling configuration
```

### External Dependencies
- **Threading**: `std::thread` for concurrent request handling
- **Filesystem**: `std::filesystem` for modern path operations
- **Network**: Platform-specific socket libraries (WinSock2/POSIX)
- **JSON**: **Custom implementation** - no external libraries required
- **String Processing**: Standard C++ `<string>`, `<sstream>`, `<regex>`

## ⚡ Performance & Memory Management

### Memory Optimization
```cpp
// Efficient patterns
std::string content = std::move(builder.buildContent());  // Move semantics
RouteHandler handler = [](const HTTPRequest& req) -> HTTPResponse {
    // Minimize copying, use references
    return HTTPResponse("200 OK");
};

// Buffer management
constexpr size_t BUFFER_SIZE = 8192;  // Fixed allocation, reused
```

### Thread Safety Considerations
- **Connection Handling**: Each client gets dedicated thread
- **Shared Data**: `ServerData` is read-only after initialization
- **State Management**: Routes are stateless by design
- **Resource Cleanup**: RAII pattern ensures proper cleanup

### Performance Characteristics
- **Route Lookup**: O(1) exact matches, O(n) wildcard patterns
- **Static Files**: Direct filesystem access, no route overhead
- **Request Processing**: Single-pass parsing, minimal allocations
- **Connection Model**: One request per connection (no keep-alive)

## 🛠️ Development Guidelines

### Code Quality Standards
1. **Cross-Platform**: Always test MSVC and MinGW builds
2. **Error Handling**: Use appropriate logging levels
3. **Resource Management**: Follow RAII principles
4. **Performance**: Profile critical paths, minimize allocations
5. **Documentation**: Comment complex algorithms and platform-specific code

### Testing Requirements
- Unit tests for all major components
- Cross-platform compatibility verification
- Performance benchmarking for routing and content building
- Memory leak detection with appropriate tools

### Common Pitfalls to Avoid
- Missing `WSACleanup()` on Windows
- Inconsistent header guards
- Platform-specific path separators
- Memory leaks in socket operations
- Incorrect JSON type assumptions (remember: all values are strings)

---

**🔧 Quick Reference**: When in doubt, check existing patterns in the codebase. This framework prioritizes consistency and cross-platform compatibility above all else.