cmake_minimum_required(VERSION 3.28)
project(Geruest_Unit_Tests VERSION 0.6.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific: Prevent min/max macros that conflict with std::min/std::max
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Configure version header (same as main project)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/geruest/Version.hpp"
)

# Fetch Google Test
# DOWNLOAD_EXTRACT_TIMESTAMP requires CMake 3.24+; this directory sets 3.28+.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

# Optional: Find CURL for email functionality
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL library found - Email functionality enabled in tests")
    set(GERUEST_HAS_CURL TRUE)
    add_compile_definitions(GERUEST_HAS_CURL=1)
else()
    message(STATUS "CURL library not found - Email functionality disabled in tests")
    set(GERUEST_HAS_CURL FALSE)
    add_compile_definitions(GERUEST_HAS_CURL=0)
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific libraries
    set(PLATFORM_LIBS ws2_32)
else()
    # Linux-specific libraries
    set(PLATFORM_LIBS pthread)
endif()

# Collect all source files from the main Geruest library
set(GERUEST_SOURCES
    ../Geruest.cpp
    ../server/Config.cpp
    ../server/Socket.cpp
    ../server/Workers.cpp
    ../server/Status.cpp
    ../auth/BasicAuth.cpp
    ../config/ConfigLoader.cpp
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../data/MethodNotAllowed.cpp
    ../parser/JSONParser.cpp
    ../FileManagement/FileManagement.cpp
    ../builders/AssetMerger.cpp
    ../builders/ContentBuilder.cpp
    ../builders/HTMLBuilder.cpp
    ../builders/CSSBuilder.cpp
    ../builders/JSBuilder.cpp
    ../builders/JSObfuscator.cpp
    ../builders/JSObfuscatorScope.cpp
    ../builders/WebPConverter.cpp
    ../handler/Handler.cpp
    ../security/Security.cpp
)

# Conditionally add EmailSender if CURL is available
if(GERUEST_HAS_CURL)
    list(APPEND GERUEST_SOURCES ../email/EmailSender.cpp)
endif()

# Create individual test executables
# Note: No longer building a combined Geruest_Unit_Tests executable
# All tests are run via individual executables or ctest
add_executable(JSONParser_Tests
    JSONParser/JSONParser_tests.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
)
target_link_libraries(JSONParser_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(HTTPRequest_Tests HTTPRequest/HTTPRequest_tests.cpp ../data/HTTPRequest.cpp)
target_link_libraries(HTTPRequest_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(HTTPResponse_Tests 
    HTTPResponse/HTTPResponse_tests.cpp 
    ../data/HTTPResponse.cpp 
    ../data/HTTPRequest.cpp
    ../data/MethodNotAllowed.cpp
)
target_link_libraries(HTTPResponse_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(FileManagement_Tests FileManagement/FileManagement_tests.cpp ../FileManagement/FileManagement.cpp)
target_link_libraries(FileManagement_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ContentBuilder_Tests 
    ContentBuilder/ContentBuilder_tests.cpp 
    ../auth/BasicAuth.cpp
    ../builders/AssetMerger.cpp
    ../builders/ContentBuilder.cpp
    ../builders/HTMLBuilder.cpp
    ../builders/CSSBuilder.cpp
    ../builders/JSBuilder.cpp
    ../builders/JSObfuscator.cpp
    ../builders/JSObfuscatorScope.cpp
    ../builders/WebPConverter.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
    ../FileManagement/FileManagement.cpp
)
target_link_libraries(ContentBuilder_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(JSObfuscator_Tests 
    JSObfuscator/JSObfuscator_tests.cpp 
    ../builders/JSObfuscator.cpp
    ../builders/JSObfuscatorScope.cpp
)
target_link_libraries(JSObfuscator_Tests ${PLATFORM_LIBS} GTest::gtest_main)
# Optional syntax check: npm i acorn && js < tools/acorn_validate_stdin.js (repo root)

add_executable(BasicAuth_Tests 
    BasicAuth/BasicAuth_tests.cpp 
    ../auth/BasicAuth.cpp
)
target_link_libraries(BasicAuth_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ConfigLoader_Tests 
    ConfigLoader/ConfigLoader_tests.cpp 
    ../config/ConfigLoader.cpp
)
target_link_libraries(ConfigLoader_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(AssetMerger_Tests 
    AssetMerger/AssetMerger_tests.cpp 
    ../builders/AssetMerger.cpp
    ../builders/WebPConverter.cpp
    ../FileManagement/FileManagement.cpp
)
target_link_libraries(AssetMerger_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(Security_Tests
    Security/Security_tests.cpp
    ../security/Security.cpp
)
target_link_libraries(Security_Tests ${PLATFORM_LIBS} GTest::gtest_main)

# Add tests to CTest using Google Test discovery
include(GoogleTest)
gtest_discover_tests(JSONParser_Tests)
gtest_discover_tests(HTTPRequest_Tests)
gtest_discover_tests(HTTPResponse_Tests)
gtest_discover_tests(FileManagement_Tests)
gtest_discover_tests(ContentBuilder_Tests)
gtest_discover_tests(JSObfuscator_Tests)
gtest_discover_tests(BasicAuth_Tests)
gtest_discover_tests(ConfigLoader_Tests)
gtest_discover_tests(AssetMerger_Tests)
gtest_discover_tests(Security_Tests)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS JSONParser_Tests HTTPRequest_Tests HTTPResponse_Tests FileManagement_Tests ContentBuilder_Tests JSObfuscator_Tests BasicAuth_Tests ConfigLoader_Tests AssetMerger_Tests Security_Tests
    COMMENT "Running all unit tests via CTest"
)

# Copy test data files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/JSONParser/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/JSONParser
     FILES_MATCHING PATTERN "*.json")
