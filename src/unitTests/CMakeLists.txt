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
    ../auth/BasicAuth.cpp
    ../config/ConfigLoader.cpp
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../parser/JSONParser.cpp
    ../FileManagement/FileManagement.cpp
    ../builders/AssetMerger.cpp
    ../builders/ContentBuilder.cpp
    ../builders/HTMLBuilder.cpp
    ../builders/CSSBuilder.cpp
    ../builders/JSBuilder.cpp
    ../builders/WebPConverter.cpp
    ../handler/Handler.cpp
)

# Conditionally add EmailSender if CURL is available
if(GERUEST_HAS_CURL)
    list(APPEND GERUEST_SOURCES ../email/EmailSender.cpp)
endif()

# Create the main unit test executable
add_executable(
    Geruest_Unit_Tests
    unit_tests.cpp
    ${GERUEST_SOURCES}
)

# Link platform-specific libraries
target_link_libraries(Geruest_Unit_Tests ${PLATFORM_LIBS})
if(GERUEST_HAS_CURL)
    target_link_libraries(Geruest_Unit_Tests CURL::libcurl)
endif()

# Create individual test executables for standalone testing
add_executable(JSONParser_Tests JSONParser/JSONParser_tests.cpp ../parser/JSONParser.cpp)
target_link_libraries(JSONParser_Tests ${PLATFORM_LIBS})

add_executable(HTTPRequest_Tests HTTPRequest/HTTPRequest_tests.cpp ../data/HTTPRequest.cpp)
target_link_libraries(HTTPRequest_Tests ${PLATFORM_LIBS})

add_executable(HTTPResponse_Tests 
    HTTPResponse/HTTPResponse_tests.cpp 
    ../data/HTTPResponse.cpp 
    ../data/HTTPRequest.cpp
)
target_link_libraries(HTTPResponse_Tests ${PLATFORM_LIBS})

add_executable(FileManagement_Tests FileManagement/FileManagement_tests.cpp ../FileManagement/FileManagement.cpp)
target_link_libraries(FileManagement_Tests ${PLATFORM_LIBS})

add_executable(ContentBuilder_Tests 
    ContentBuilder/ContentBuilder_tests.cpp 
    ../auth/BasicAuth.cpp
    ../builders/AssetMerger.cpp
    ../builders/ContentBuilder.cpp
    ../builders/HTMLBuilder.cpp
    ../builders/CSSBuilder.cpp
    ../builders/JSBuilder.cpp
    ../builders/WebPConverter.cpp
    ../parser/JSONParser.cpp
    ../FileManagement/FileManagement.cpp
)
target_link_libraries(ContentBuilder_Tests ${PLATFORM_LIBS})

# Add tests to CTest
add_test(NAME JSONParser_Tests COMMAND JSONParser_Tests)
add_test(NAME HTTPRequest_Tests COMMAND HTTPRequest_Tests)
add_test(NAME HTTPResponse_Tests COMMAND HTTPResponse_Tests)
add_test(NAME FileManagement_Tests COMMAND FileManagement_Tests)
add_test(NAME ContentBuilder_Tests COMMAND ContentBuilder_Tests)
add_test(NAME All_Unit_Tests COMMAND Geruest_Unit_Tests)

# Set test properties
set_tests_properties(JSONParser_Tests PROPERTIES TIMEOUT 30)
set_tests_properties(HTTPRequest_Tests PROPERTIES TIMEOUT 30)
set_tests_properties(HTTPResponse_Tests PROPERTIES TIMEOUT 30)
set_tests_properties(FileManagement_Tests PROPERTIES TIMEOUT 30)
set_tests_properties(ContentBuilder_Tests PROPERTIES TIMEOUT 60)
set_tests_properties(All_Unit_Tests PROPERTIES TIMEOUT 300)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS Geruest_Unit_Tests JSONParser_Tests HTTPRequest_Tests HTTPResponse_Tests FileManagement_Tests ContentBuilder_Tests
    COMMENT "Running all unit tests"
)

# Copy test data files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/JSONParser/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/JSONParser
     FILES_MATCHING PATTERN "*.json")
