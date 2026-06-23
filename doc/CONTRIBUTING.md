# Contributing

## Setup

**Prerequisites:** C++20 compiler (GCC 10+, Clang 11+), CMake 3.11+, Git, **Boost** (`libboost-system-dev` on Debian/Ubuntu)

```bash
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest
git remote add upstream https://github.com/UBehrmann/Geruest.git

# Linux/Unix — library + unit tests from one build tree
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DGERUEST_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

Standalone test project (optional; requires CMake 3.28+):

```bash
cd src/unitTests && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest --output-on-failure
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

**Platform:**
```cpp
int fd = -1;
close(fd);
```

**Include Order:** Related header → C++ std → System/platform → Project headers

## Testing

Tests use **Google Test** (fetched automatically by `src/unitTests/CMakeLists.txt`). Follow the pattern in existing files such as `src/unitTests/Security/Security_tests.cpp`:

```cpp
#include <gtest/gtest.h>
#include "../../security/Security.hpp"

using namespace geruest;

TEST(SecurityTest, EscapeHtmlEscapesAngleBrackets) {
    EXPECT_EQ(Security::escapeHtml("<script>"), "&lt;script&gt;");
}
```

Register a new executable in `src/unitTests/CMakeLists.txt`:

```cmake
add_executable(MyFeature_Tests MyFeature/MyFeature_tests.cpp)
target_link_libraries(MyFeature_Tests Geruest::Core GTest::gtest_main)
gtest_discover_tests(MyFeature_Tests)
```

Use `Geruest::Geruest`, `Geruest::Assets`, or another module target when the code under test lives outside Core.

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

**Checklist:** ☑ Compiles (Linux/Unix) ☑ Tests pass ☑ New tests added ☑ Docs updated ☑ No warnings ☑ Follows style

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
