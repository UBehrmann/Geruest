# Geruest Framework Unit Tests

This directory contains comprehensive unit tests for the Geruest C++ web framework. The tests are organized in a modular structure with separate test suites for each major component.

## Test Structure

All tests use Google Test framework with individual executables for each module.

```
unitTests/
├── CMakeLists.txt                     # Build configuration (Google Test)
├── JSONParser/                        # JSONParser tests
│   ├── JSONParser_tests.cpp
│   └── *.json                         # Test data files
├── HTTPRequest/                       # HTTPRequest tests
│   └── HTTPRequest_tests.cpp
├── HTTPResponse/                      # HTTPResponse tests
│   └── HTTPResponse_tests.cpp
├── FileManagement/                    # FileManagement tests
│   └── FileManagement_tests.cpp
├── ContentBuilder/                    # ContentBuilder tests
│   └── ContentBuilder_tests.cpp
└── JSObfuscator/                      # JSObfuscator tests
    └── JSObfuscator_tests.cpp
```

## Building and Running Tests

### Prerequisites
- CMake 3.28 or higher
- C++20 compatible compiler (GCC, Clang)
- **Boost** (system component) for the server lifecycle tests
- Platform-specific dependencies:
  - Linux/Unix: `pthread` library

### Build Commands

#### Full Test Suite
```bash
# Create and enter build directory
mkdir build && cd build

# Configure (Linux)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build all tests
cmake --build . --config Release
```

#### Run All Tests
```bash
# Navigate to build directory
cd build

# Run all tests via CTest (recommended)
ctest --output-on-failure

# Or with verbose output
ctest --output-on-failure --verbose
```

#### Run Individual Test Suites
```bash
# From the build directory

# JSONParser tests only (38 tests)
./JSONParser_Tests

# HTTPRequest tests only (6 tests)
./HTTPRequest_Tests

# HTTPResponse tests only (8 tests)
./HTTPResponse_Tests

# FileManagement tests only (9 tests)
./FileManagement_Tests

# ContentBuilder tests only (8 tests)
./ContentBuilder_Tests

# JSObfuscator tests only (14 tests)
./JSObfuscator_Tests

# BasicAuth tests only (34 tests)
./BasicAuth_Tests

# ConfigLoader tests only (56 tests)
./ConfigLoader_Tests

# AssetMerger tests only (18 tests)
./AssetMerger_Tests

# Run specific test with filter
./JSONParser_Tests --gtest_filter=JSONParserTest.SimpleKeyStrings
```

## Test Modules

### 1. JSONParser Tests (38 tests)
- **Location**: `JSONParser/JSONParser_tests.cpp`
- **Test Data**: Various `.json` files for different scenarios
- **Coverage**: 
  - String, integer, float, boolean parsing
  - Array handling (all data types)
  - Nested objects
  - File I/O operations
  - Error handling for invalid JSON

### 2. HTTPRequest Tests (6 tests)
- **Location**: `HTTPRequest/HTTPRequest_tests.cpp`
- **Coverage**:
  - HTTP method parsing (GET, POST, etc.)
  - Header parsing (case-insensitive)
  - Body content extraction
  - Query parameter handling
  - URL decoding

### 3. HTTPResponse Tests (8 tests)
- **Location**: `HTTPResponse/HTTPResponse_tests.cpp` 
- **Coverage**:
  - Response construction
  - Header management (set/add)
  - Body content handling
  - Predefined response functions
  - CORS header generation

### 4. FileManagement Tests (9 tests)
- **Location**: `FileManagement/FileManagement_tests.cpp`
- **Coverage**:
  - File creation and deletion
  - Directory creation
  - File existence checking
  - File content saving/loading
  - Path handling and validation

### 5. ContentBuilder Tests (8 tests)
- **Location**: `ContentBuilder/ContentBuilder_tests.cpp`
- **Coverage**:
  - File loading functionality
  - Comment removal (HTML, CSS, JS)
  - Content building pipeline
  - Template processing capabilities

### 6. JSObfuscator Tests (14 tests)
- **Location**: `JSObfuscator/JSObfuscator_tests.cpp`
- **Coverage**:
  - Code obfuscation levels (1-3)
  - String encoding and encryption
  - Variable and function name mangling
  - Dead code injection
  - Syntax preservation

### 7. BasicAuth Tests (34 tests)
- **Location**: `BasicAuth/BasicAuth_tests.cpp`
- **Coverage**:
  - User management (add/remove/clear users)
  - Protected page management
  - Password hashing (SHA-256)
  - Credential verification
  - Authorization header parsing (Base64 decode)
  - Authentication flow logic
  - Edge cases (empty credentials, special characters)

### 8. ConfigLoader Tests (56 tests)
- **Location**: `ConfigLoader/ConfigLoader_tests.cpp`
- **Coverage**:
  - .env file loading and parsing
  - Environment variable fallback
  - Type conversions (int, float, bool, size_t)
  - Default value handling
  - Comment and whitespace handling
  - Configuration hierarchy (code > .env > env > default)
  - Multiple file loads
  - Edge cases (equals signs in values, empty keys)

### 9. AssetMerger Tests (18 tests)
- **Location**: `AssetMerger/AssetMerger_tests.cpp`
- **Coverage**:
  - CSS file merging and bundling
  - JavaScript file merging and bundling
  - External URL filtering (CDN links preserved)
  - HTML tag modification and replacement
  - Path resolution and subdirectory handling
  - Asset tag attribute handling
  - Multiple file merging scenarios

## CI/CD Integration

The unit tests are designed to work in continuous integration environments:

### Exit Codes
- `0`: All tests passed
- `1`: One or more tests failed

### Test Output Format
- ✓ Indicates passed tests
- ✗ Indicates failed tests  
- Detailed failure messages with error descriptions
- Summary statistics (tests run, passed, failed)

### Example CI Usage
```yaml
# GitHub Actions example
- name: Build and Run Tests
  run: |
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    ctest --output-on-failure
```

## Adding New Tests

### Creating a New Test Module
1. Create a new directory under `unitTests/`
2. Create the test file following the naming pattern `ComponentName_tests.cpp`
3. Use the standard test template:

```cpp
namespace geruest {
namespace test {

void test_function_name() {
    // Test implementation
    assert(condition);
}

bool runComponentNameTests() {
    // Test runner implementation
    // Return true if all tests pass
}

} // namespace test
} // namespace geruest

#ifndef RUNNING_MAIN_TESTS
int main() {
    return geruest::test::runComponentNameTests() ? 0 : 1;
}
#endif
```

4. Update `unit_tests.cpp` to include the new test module
5. Update `CMakeLists.txt` to build the new test executable

### Test Best Practices
- Use descriptive test function names
- Include both positive and negative test cases
- Test edge cases and error conditions  
- Clean up any created files/resources
- Use assertions to validate expected behavior
- Keep tests isolated and independent

## Troubleshooting

### Common Issues
1. **Include Path Errors**: Ensure relative paths are correct (`../component/header.hpp`)
2. **Linking Errors**: Verify all required source files are included in CMakeLists.txt
3. **Platform-Specific Issues**: Check that `pthread` is linked
4. **File Path Issues**: Use canonical Unix-style paths

### Debug Mode
Build tests in debug mode for more detailed error information:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

## Contributing

When adding new features to Geruest:
1. Create corresponding unit tests
2. Ensure existing tests still pass
3. Update this README if new test modules are added
4. Follow the established testing patterns and conventions