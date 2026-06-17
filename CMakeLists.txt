cmake_minimum_required(VERSION 3.11)

# Define the project name and version
project(GeruestLibrary VERSION 0.11.6 LANGUAGES CXX)

# Configure version header
configure_file(
    "${PROJECT_SOURCE_DIR}/src/Version.hpp.in"
    "${PROJECT_BINARY_DIR}/include/geruest/Version.hpp"
)

# C++20 required for Boost.Asio coroutines (co_await). Boost >= 1.75 recommended.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Geruest currently supports Linux/Unix targets only.
if(WIN32)
    message(FATAL_ERROR "Windows is no longer supported. Build on Linux/Unix.")
endif()

# Enable more warnings for all compilers
add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)

# Output static libs to /lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Required packages
find_package(Threads REQUIRED)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost 1.75 REQUIRED COMPONENTS system)

option(GERUEST_ENABLE_POSTGRESQL "Enable PostgreSQL backend support" OFF)
option(GERUEST_ENABLE_SQLITE "Enable SQLite backend support" OFF)

# Optional: Find CURL for email functionality
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL library found - Email functionality enabled")
    set(GERUEST_HAS_CURL TRUE)
else()
    message(STATUS "CURL library not found - Email functionality disabled")
    message(STATUS "To enable email functionality, install libcurl4-openssl-dev (apt) or curl (vcpkg)")
    set(GERUEST_HAS_CURL FALSE)
endif()

# Optional: Find PostgreSQL (libpq) for database backend
set(GERUEST_HAS_LIBPQ FALSE)
if(GERUEST_ENABLE_POSTGRESQL)
    find_package(PostgreSQL QUIET)
    if(PostgreSQL_FOUND)
        message(STATUS "PostgreSQL library found - PostgreSQL backend enabled")
        set(GERUEST_HAS_LIBPQ TRUE)
    else()
        message(STATUS "PostgreSQL requested but not found - PostgreSQL backend disabled")
        message(STATUS "To enable PostgreSQL backend, install libpq-dev (apt)")
    endif()
endif()

# Optional: Find SQLite for database backend
set(GERUEST_HAS_SQLITE FALSE)
if(GERUEST_ENABLE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        message(STATUS "SQLite library found - SQLite backend enabled")
        set(GERUEST_HAS_SQLITE TRUE)
    else()
        message(STATUS "SQLite requested but not found - SQLite backend disabled")
        message(STATUS "To enable SQLite backend, install libsqlite3-dev (apt)")
    endif()
endif()

# Optional: Find libwebp for WebP image conversion
# Users must install libwebp-dev (Linux) or webp (vcpkg/conan) for this feature
find_package(WebP QUIET)
if(WebP_FOUND)
    message(STATUS "WebP library found via CMake config - WebP conversion enabled")
    set(GERUEST_HAS_WEBP TRUE)
    set(WEBP_LINK_TARGET WebP::webp)
else()
    # Fallback to pkg-config (required for libwebp-dev on Ubuntu/Debian)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBWEBP QUIET libwebp)
        if(LIBWEBP_FOUND)
            message(STATUS "WebP library found via pkg-config - WebP conversion enabled")
            set(GERUEST_HAS_WEBP TRUE)
        endif()
    endif()
    
    if(NOT GERUEST_HAS_WEBP)
        message(STATUS "WebP library not found - WebP conversion disabled")
        message(STATUS "To enable WebP conversion, install libwebp-dev (apt) or webp (vcpkg)")
        set(GERUEST_HAS_WEBP FALSE)
    endif()
endif()

# Create the static library
add_library(Geruest STATIC
    src/Geruest.cpp
    src/server/Config.cpp
    src/server/Socket.cpp
    src/server/Workers.cpp
    src/server/HttpSession.cpp
    src/server/WebSocket.cpp
    src/server/Status.cpp
    src/auth/BasicAuth.cpp
    src/builders/AssetMerger.cpp
    src/builders/ContentBuilder.cpp
    src/builders/CSSBuilder.cpp
    src/builders/HTMLBuilder.cpp
    src/builders/JSBuilder.cpp
    src/builders/JSObfuscator.cpp
    src/builders/JSObfuscatorScope.cpp
    src/builders/WebPConverter.cpp
    src/config/ConfigLoader.cpp
    src/data/HTTPRequest.cpp
    src/data/HTTPResponse.cpp
    src/data/MethodNotAllowed.cpp
    src/data/ServerData.cpp
    src/email/EmailSender.cpp
    src/FileManagement/FileManagement.cpp
    src/handler/Handler.cpp
    src/parser/JSONParser.cpp
    src/security/Security.cpp
)

if(GERUEST_HAS_LIBPQ OR GERUEST_HAS_SQLITE)
    target_sources(Geruest PRIVATE
        src/database/DatabaseClient.cpp
        src/database/DbExecutor.cpp
    )
endif()

# Public headers
target_include_directories(Geruest PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Geruest PUBLIC Threads::Threads)
target_link_libraries(Geruest PUBLIC Boost::system)

# Link CURL library if found (PUBLIC because EmailSender needs it)
if(GERUEST_HAS_CURL)
    target_link_libraries(Geruest PUBLIC CURL::libcurl)
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_CURL=1)
else()
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_CURL=0)
endif()

if(GERUEST_HAS_LIBPQ)
    target_link_libraries(Geruest PUBLIC PostgreSQL::PostgreSQL)
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_LIBPQ=1)
else()
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_LIBPQ=0)
endif()

if(GERUEST_HAS_SQLITE)
    target_link_libraries(Geruest PUBLIC SQLite::SQLite3)
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_SQLITE=1)
else()
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_SQLITE=0)
endif()

# Link WebP library if found (PRIVATE to avoid leaking to consumers)
if(GERUEST_HAS_WEBP)
    if(WEBP_LINK_TARGET)
        # CMake config target (vcpkg, conan, etc.)
        target_link_libraries(Geruest PRIVATE ${WEBP_LINK_TARGET})
    else()
        # pkg-config variables (libwebp-dev on Ubuntu/Debian)
        target_include_directories(Geruest PRIVATE ${LIBWEBP_INCLUDE_DIRS})
        target_link_libraries(Geruest PRIVATE ${LIBWEBP_LIBRARIES})
        target_link_directories(Geruest PRIVATE ${LIBWEBP_LIBRARY_DIRS})
    endif()
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_WEBP=1)
else()
    target_compile_definitions(Geruest PUBLIC GERUEST_HAS_WEBP=0)
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
