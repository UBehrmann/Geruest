# Contributing to Geruest

Thank you for your interest in contributing to Geruest! This document provides guidelines for contributing to the project.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Project Architecture](#project-architecture)
- [Code Style Guidelines](#code-style-guidelines)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Adding New Features](#adding-new-features)

---

## Getting Started

### Prerequisites

- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake** 3.10 or higher
- **Git** for version control
- Basic understanding of socket programming
- Familiarity with HTTP protocol

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/Geruest.git
   cd Geruest
   ```
3. Add upstream remote:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/Geruest.git
   ```

---

## Development Setup

### Linux

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run tests
cd ../src/unitTests
mkdir build && cd build
cmake ..
make
./Geruest_Unit_Tests
```

### Windows (MSVC)

```powershell
# Open Developer PowerShell
mkdir build && cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Run tests
cd ..\src\unitTests
mkdir build && cd build
cmake .. -A x64
cmake --build .
.\Debug\Geruest_Unit_Tests.exe
```

### VS Code Tasks

The project includes VS Code tasks for common operations:

- **Build Library (Unix/MinGW)** - Build the library on Linux/MinGW
- **Build Library (MSVC)** - Build the library on Windows with MSVC
- **Build and Run All Unit Tests** - Run all unit tests
- **Build and Run Example** - Build and run the example server

Use `Ctrl+Shift+B` to access build tasks.

---

## Project Architecture

### Directory Structure

```
Geruest/
├── src/                    # Source files
│   ├── Geruest.cpp/hpp     # Main server class
│   ├── auth/               # Authentication
│   │   └── BasicAuth.cpp/hpp
│   ├── builders/           # Content builders
│   │   ├── AssetMerger.cpp/hpp
│   │   ├── ContentBuilder.cpp/hpp
│   │   ├── CSSBuilder.cpp/hpp
│   │   ├── HTMLBuilder.cpp/hpp
│   │   └── JSBuilder.cpp/hpp
│   ├── data/               # Data structures
│   │   ├── HTTPRequest.cpp/hpp
│   │   ├── HTTPResponse.cpp/hpp
│   │   └── ServerData.hpp
│   ├── FileManagement/     # File utilities
│   │   └── FileManagement.cpp/hpp
│   ├── handler/            # Request handling
│   │   └── Handler.cpp/hpp
│   ├── parser/             # JSON parser
│   │   └── JSONParser.cpp/hpp
│   └── unitTests/          # Unit tests
├── exemple/                # Example application
├── doc/                    # Documentation
├── cmake/                  # CMake config files
└── CMakeLists.txt          # Main build config
```

### Core Components

#### Geruest (Main Server)

The main entry point. Handles:
- Socket initialization
- Thread pool management
- Route registration
- Server lifecycle

#### Handler

Processes individual requests:
- Receives raw HTTP data
- Parses into HTTPRequest
- Routes to handlers or static files
- Sends HTTPResponse

#### ServerData

Shared configuration storage:
- Routes (exact and wildcard)
- Server root path
- Languages
- Feature flags

#### HTTPRequest / HTTPResponse

Data classes for HTTP communication:
- Request parsing
- Response building
- Header management

#### ContentBuilder Hierarchy

```
ContentBuilder (base)
├── HTMLBuilder    - Processes HTML templates
├── CSSBuilder     - Processes CSS files
└── JSBuilder      - Processes JavaScript files

AssetMerger        - Merges CSS/JS assets
```

#### JSONParser

Custom JSON implementation:
- No external dependencies
- String-based storage
- Type-safe accessors

---

## Code Style Guidelines

### General Principles

1. **Clarity over cleverness** - Write readable code
2. **Consistency** - Follow existing patterns
3. **Comments** - Explain "why", not "what"
4. **Cross-platform** - Test on Windows and Linux

### Naming Conventions

```cpp
// Classes: PascalCase
class HTTPRequest { };
class ContentBuilder { };

// Methods: camelCase
void addRoute();
std::string getHeader();

// Member variables: underscore prefix for private
private:
    std::string _root;
    bool _mergeAssets;

// Constants: SCREAMING_SNAKE_CASE
#define BUFFER_SIZE 8192
static constexpr const char* FILETYPE_HTML = "html";

// Namespaces: lowercase
namespace geruest { }
```

### Header Guards

Use `#ifndef` style guards:

```cpp
#ifndef GERUEST_CLASSNAME_HPP
#define GERUEST_CLASSNAME_HPP

// ... content ...

#endif // GERUEST_CLASSNAME_HPP
```

### Cross-Platform Code

Always handle platform differences:

```cpp
#ifdef _WIN32
    SOCKET server_fd = INVALID_SOCKET;
    // Windows-specific code
    closesocket(server_fd);
#else
    int server_fd = -1;
    // POSIX code
    close(server_fd);
#endif
```

### Include Order

1. Related header (for .cpp files)
2. C++ standard library
3. System/platform headers
4. Project headers

```cpp
// In HTTPRequest.cpp
#include "HTTPRequest.hpp"

#include <algorithm>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "parser/JSONParser.hpp"
```

### Documentation

Use Doxygen-style comments:

```cpp
/**
 * @file ClassName.hpp
 * @date DD.MM.YYYY
 * @author Your Name
 * @brief Brief description of the file
 */

/**
 * @brief Brief description of the method
 * @param paramName Description of parameter
 * @return Description of return value
 * @note Additional notes
 */
ReturnType methodName(ParamType paramName);
```

---

## Testing

### Unit Test Structure

Tests are in `src/unitTests/`:

```
unitTests/
├── CMakeLists.txt
├── unit_tests.cpp       # Main test runner
├── JSONParser/
│   └── JSONParser_tests.cpp
├── HTTPRequest/
│   └── HTTPRequest_tests.cpp
├── HTTPResponse/
│   └── HTTPResponse_tests.cpp
├── ContentBuilder/
│   └── ContentBuilder_tests.cpp
└── FileManagement/
    └── FileManagement_tests.cpp
```

### Writing Tests

```cpp
// Example test structure
#include <iostream>
#include <cassert>
#include "../parser/JSONParser.hpp"

void testJSONParserBasic() {
    std::cout << "Testing JSONParser basic operations..." << std::endl;
    
    geruest::JSONParser json(R"({"name": "test", "value": 42})");
    
    assert(json.getString("name") == "test");
    assert(json.getInt("value") == 42);
    
    std::cout << "  ✓ Basic operations passed" << std::endl;
}

void testJSONParserArrays() {
    std::cout << "Testing JSONParser arrays..." << std::endl;
    
    geruest::JSONParser json(R"({"items": [1, 2, 3]})");
    
    auto items = json.getIntArray("items");
    assert(items.size() == 3);
    assert(items[0] == 1);
    
    std::cout << "  ✓ Array operations passed" << std::endl;
}

int main() {
    std::cout << "=== JSONParser Tests ===" << std::endl;
    
    testJSONParserBasic();
    testJSONParserArrays();
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
```

### Running Tests

```bash
# Linux
cd src/unitTests
mkdir build && cd build
cmake ..
make
./Geruest_Unit_Tests

# Individual test executables
./JSONParser_Tests
./HTTPRequest_Tests
./ContentBuilder_Tests

# Windows
cd src\unitTests
mkdir build && cd build
cmake .. -A x64
cmake --build .
.\Debug\Geruest_Unit_Tests.exe
```

### Adding New Tests

1. Create test file in appropriate subdirectory
2. Add to `CMakeLists.txt`:
   ```cmake
   add_executable(NewFeature_Tests
       NewFeature/NewFeature_tests.cpp
       ../path/to/NewFeature.cpp
   )
   ```
3. Include in main test runner if needed

---

## Submitting Changes

### Branch Naming

- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation
- `refactor/description` - Code refactoring

### Commit Messages

Follow conventional commits:

```
type(scope): subject

body (optional)

footer (optional)
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting
- `refactor`: Code restructuring
- `test`: Adding tests
- `chore`: Maintenance

Examples:
```
feat(auth): add SHA-256 password hashing

fix(handler): resolve memory leak in request parsing

docs(readme): update installation instructions

refactor(json): optimize string parsing performance
```

### Pull Request Process

1. **Update your fork**:
   ```bash
   git fetch upstream
   git checkout main
   git merge upstream/main
   ```

2. **Create feature branch**:
   ```bash
   git checkout -b feature/my-feature
   ```

3. **Make changes and commit**:
   ```bash
   git add .
   git commit -m "feat(scope): description"
   ```

4. **Push to your fork**:
   ```bash
   git push origin feature/my-feature
   ```

5. **Create Pull Request** on GitHub

6. **Address review feedback**

### PR Checklist

- [ ] Code compiles on Linux (GCC)
- [ ] Code compiles on Windows (MSVC)
- [ ] All existing tests pass
- [ ] New tests added for new features
- [ ] Documentation updated if needed
- [ ] No compiler warnings
- [ ] Code follows style guidelines

---

## Adding New Features

### Feature Development Workflow

1. **Discuss first** - Open an issue to discuss the feature
2. **Design** - Plan the implementation
3. **Implement** - Write the code
4. **Test** - Add comprehensive tests
5. **Document** - Update documentation
6. **Submit** - Create pull request

### Example: Adding a New Builder

1. **Create header** (`src/builders/NewBuilder.hpp`):

```cpp
#ifndef GERUEST_NEWBUILDER_HPP
#define GERUEST_NEWBUILDER_HPP

#include "ContentBuilder.hpp"

namespace geruest {

class NewBuilder : public ContentBuilder {
public:
    NewBuilder(const std::string& inputPath, 
               const std::string& inputServerRoot,
               bool removeCommentsFlag = true);

private:
    void buildContent();
};

}  // namespace geruest

#endif  // GERUEST_NEWBUILDER_HPP
```

2. **Create implementation** (`src/builders/NewBuilder.cpp`):

```cpp
#include "NewBuilder.hpp"

namespace geruest {

NewBuilder::NewBuilder(const std::string& inputPath,
                       const std::string& inputServerRoot,
                       bool removeCommentsFlag)
    : ContentBuilder(inputPath, inputServerRoot, removeCommentsFlag) {
    buildContent();
}

void NewBuilder::buildContent() {
    builtFile = loadFile(path);
    
    if (removeComments) {
        builtFile = removeCommentsFromString(builtFile, "new");
    }
    
    // Custom processing...
}

}  // namespace geruest
```

3. **Add to CMakeLists.txt**:

```cmake
add_library(Geruest STATIC
    # ... existing files ...
    src/builders/NewBuilder.cpp
)
```

4. **Add tests** (`src/unitTests/NewBuilder/NewBuilder_tests.cpp`)

5. **Update documentation**

### Example: Adding Server Configuration

1. **Add to Geruest.hpp**:

```cpp
class Geruest {
public:
    // ... existing ...
    
    /**
     * @brief Sets the new configuration option
     * @param value The value to set
     * @note Must be called before init()
     */
    void setNewOption(bool value);
};
```

2. **Implement in Geruest.cpp**:

```cpp
void Geruest::setNewOption(bool value) {
    serverData.setNewOption(value);
}
```

3. **Add to ServerData.hpp**:

```cpp
class ServerData {
private:
    bool _newOption = false;  // Default value
    
public:
    void setNewOption(bool value) { _newOption = value; }
    bool getNewOption() const { return _newOption; }
};
```

4. **Use in Handler** or relevant component

---

## Common Tasks

### Adding a New HTTP Response Helper

In `HTTPResponse.hpp`:

```cpp
// 418 I'm a teapot (example)
[[maybe_unused]] HTTPResponse responseTeapot(const HTTPRequest* request = nullptr);
```

In `HTTPResponse.cpp`:

```cpp
HTTPResponse responseTeapot(const HTTPRequest* request) {
    HTTPResponse response("418 I'm a teapot");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("I'm a teapot");
    return response;
}
```

### Adding a JSONParser Type

In `JSONParser.hpp`:

```cpp
// Getter
unsigned int getUnsignedInt(const std::string& key);

// Setter
void setUnsignedInt(const std::string& key, unsigned int value);
```

In `JSONParser.cpp`:

```cpp
unsigned int JSONParser::getUnsignedInt(const std::string& key) {
    return static_cast<unsigned int>(std::stoul(data[key]));
}

void JSONParser::setUnsignedInt(const std::string& key, unsigned int value) {
    data[key] = std::to_string(value);
    keys.push_back(key);
}
```

---

## Getting Help

- **Issues** - Report bugs or request features
- **Discussions** - Ask questions or share ideas
- **Documentation** - Check existing docs first

Thank you for contributing to Geruest!
