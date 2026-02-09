# Contributing

## Setup

**Prerequisites:** C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+), CMake 3.10+, Git

```bash
git clone https://github.com/YOUR_USERNAME/Geruest.git && cd Geruest
git remote add upstream https://github.com/ORIGINAL_OWNER/Geruest.git

# Linux/macOS
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j$(nproc)

# Windows (MSVC)
mkdir build && cd build && cmake .. -A x64 -DCMAKE_BUILD_TYPE=Debug && cmake --build .

# Test
cd src/unitTests && mkdir build && cd build && cmake .. && make && ./Geruest_Unit_Tests
```

## Code Style

**Naming:** Classes `PascalCase`, methods `camelCase`, private `_underscore`, constants `SCREAMING_SNAKE_CASE`

**Header Guards:**
```cpp
#ifndef GERUEST_CLASSNAME_HPP
#define GERUEST_CLASSNAME_HPP
// ...
#endif
```

**Cross-Platform:**
```cpp
#ifdef _WIN32
    SOCKET fd = INVALID_SOCKET;
    closesocket(fd);
#else
    int fd = -1;
    close(fd);
#endif
```

**Include Order:** Related header → C++ std → System/platform → Project headers

## Testing

```cpp
#include <iostream>
#include <cassert>

void testFeature() {
    assert(result == expected);
    std::cout << "  ✓ Passed" << std::endl;
}
```

Add to `CMakeLists.txt`:
```cmake
add_executable(Feature_Tests Feature/tests.cpp ../path/to/Feature.cpp)
```

## Pull Requests

**Branch:** `feature/desc`, `fix/desc`, `docs/desc`, `refactor/desc`

**Commit:** `type(scope): subject`  
Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

```bash
git fetch upstream && git checkout main && git merge upstream/main
git checkout -b feature/my-feature
git commit -m "feat(scope): description"
git push origin feature/my-feature
# Open PR on GitHub
```

**Checklist:** ☑ Compiles (Linux + Windows) ☑ Tests pass ☑ New tests added ☑ Docs updated ☑ No warnings ☑ Follows style

## Common Tasks

**Add HTTP Response:**
```cpp
// HTTPResponse.hpp
HTTPResponse responseTeapot(const HTTPRequest* request = nullptr);

// HTTPResponse.cpp
HTTPResponse responseTeapot(const HTTPRequest* request) {
    HTTPResponse response("418 I'm a teapot");
    response.setBody("I'm a teapot");
    return response;
}
```

**Add JSONParser Type:**
```cpp
// .hpp
unsigned int getUnsignedInt(const std::string& key);
void setUnsignedInt(const std::string& key, unsigned int value);

// .cpp
unsigned int JSONParser::getUnsignedInt(const std::string& key) {
    return static_cast<unsigned int>(std::stoul(data[key]));
}
void JSONParser::setUnsignedInt(const std::string& key, unsigned int value) {
    data[key] = std::to_string(value);
    keys.push_back(key);
}
```

**Add Server Config:**
```cpp
// Geruest.hpp: void setNewOption(bool value);
// Geruest.cpp: void Geruest::setNewOption(bool v) { serverData.setNewOption(v); }
// ServerData.hpp: bool _newOption = false; void setNewOption(bool v) { _newOption = v; }
```
