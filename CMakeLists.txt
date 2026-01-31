cmake_minimum_required(VERSION 3.10)

# Define the project name and version
project(GeruestLibrary VERSION 0.6.5 LANGUAGES CXX)

# Configure version header
configure_file(
    "${PROJECT_SOURCE_DIR}/src/Version.hpp.in"
    "${PROJECT_BINARY_DIR}/include/geruest/Version.hpp"
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable static linking for MinGW
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
endif()

# Enable more warnings for all compilers
if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Output static libs to /lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Required packages
find_package(Threads REQUIRED)

# Create the static library
add_library(Geruest STATIC
    src/Geruest.cpp
    src/auth/BasicAuth.cpp
    src/builders/AssetMerger.cpp
    src/builders/ContentBuilder.cpp
    src/builders/CSSBuilder.cpp
    src/builders/HTMLBuilder.cpp
    src/builders/JSBuilder.cpp
    src/data/HTTPRequest.cpp
    src/data/HTTPResponse.cpp
    src/FileManagement/FileManagement.cpp
    src/handler/Handler.cpp
    src/parser/JSONParser.cpp
)

# Public headers
target_include_directories(Geruest PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Geruest Threads::Threads)
if(WIN32)
    target_link_libraries(Geruest ws2_32)
endif()

# -------------------
# Installation rules
# -------------------

# Install the library and export config
install(TARGETS Geruest
    EXPORT GeruestTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(DIRECTORY src/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Install generated version header
install(FILES "${PROJECT_BINARY_DIR}/include/geruest/Version.hpp"
    DESTINATION include/geruest
)

# Export the targets
install(EXPORT GeruestTargets
    FILE GeruestTargets.cmake
    NAMESPACE Geruest::
    DESTINATION lib/cmake/Geruest
)

# Generate config + version files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GeruestConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Geruest
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfigVersion.cmake"
    DESTINATION lib/cmake/Geruest
)
