cmake_minimum_required(VERSION 3.11)

project(GeruestLibrary VERSION 0.13.3 LANGUAGES CXX)

set(GERUEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(GERUEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

configure_file(
    "${GERUEST_ROOT}/src/Version.hpp.in"
    "${GERUEST_BINARY_DIR}/include/geruest/Version.hpp"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    message(FATAL_ERROR "Windows is no longer supported. Build on Linux/Unix.")
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Threads REQUIRED)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost 1.75 REQUIRED COMPONENTS system)

option(GERUEST_ENABLE_POSTGRESQL "Enable PostgreSQL backend support" OFF)
option(GERUEST_ENABLE_SQLITE "Enable SQLite backend support" OFF)
option(GERUEST_ENABLE_WEBSOCKET "Build WebSocket module" ON)
option(GERUEST_ENABLE_ASSETS "Build Assets module (HTML merge, WebP)" ON)
option(GERUEST_ENABLE_OBFUSCATION "Build Obfuscation module" ON)
option(GERUEST_ENABLE_EMAIL "Build Email module (requires CURL)" ON)
option(GERUEST_ENABLE_WEBP "Enable WebP image conversion (requires libwebp)" OFF)

find_package(CURL QUIET)
if(CURL_FOUND)
    set(GERUEST_HAS_CURL TRUE)
else()
    set(GERUEST_HAS_CURL FALSE)
    set(GERUEST_ENABLE_EMAIL OFF)
endif()

set(GERUEST_HAS_LIBPQ FALSE)
if(GERUEST_ENABLE_POSTGRESQL)
    find_package(PostgreSQL QUIET)
    if(PostgreSQL_FOUND)
        set(GERUEST_HAS_LIBPQ TRUE)
    endif()
endif()

set(GERUEST_HAS_SQLITE FALSE)
if(GERUEST_ENABLE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        set(GERUEST_HAS_SQLITE TRUE)
    endif()
endif()

find_package(ZLIB REQUIRED)

set(GERUEST_HAS_WEBP FALSE)
if(GERUEST_ENABLE_WEBP)
    find_package(WebP QUIET)
    if(WebP_FOUND)
        set(GERUEST_HAS_WEBP TRUE)
        set(WEBP_LINK_TARGET WebP::webp)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBWEBP QUIET libwebp)
            if(LIBWEBP_FOUND)
                set(GERUEST_HAS_WEBP TRUE)
            endif()
        endif()
    endif()
endif()

foreach(_flag CURL LIBPQ SQLITE WEBP)
    if(GERUEST_HAS_${_flag})
        set(GERUEST_HAS_${_flag}_NUM 1)
    else()
        set(GERUEST_HAS_${_flag}_NUM 0)
    endif()
endforeach()
unset(_flag)

foreach(_mod WEBSOCKET ASSETS OBFUSCATION EMAIL WEBP)
    if(GERUEST_ENABLE_${_mod})
        set(GERUEST_ENABLE_${_mod}_NUM 1)
    else()
        set(GERUEST_ENABLE_${_mod}_NUM 0)
    endif()
endforeach()
unset(_mod)

configure_file(
    "${GERUEST_ROOT}/src/geruest/BuildConfig.hpp.in"
    "${GERUEST_BINARY_DIR}/include/geruest/BuildConfig.hpp"
)

include(${GERUEST_ROOT}/cmake/GeruestModules.cmake)

set(GERUEST_CMAKE_FIND_DEPS "find_dependency(Threads REQUIRED)\nfind_dependency(Boost 1.75 REQUIRED COMPONENTS system)\n")
if(GERUEST_HAS_CURL AND GERUEST_ENABLE_EMAIL)
    string(APPEND GERUEST_CMAKE_FIND_DEPS "find_dependency(CURL REQUIRED)\n")
endif()
if(GERUEST_HAS_LIBPQ)
    string(APPEND GERUEST_CMAKE_FIND_DEPS "find_dependency(PostgreSQL REQUIRED)\n")
endif()
if(GERUEST_HAS_SQLITE)
    string(APPEND GERUEST_CMAKE_FIND_DEPS "find_dependency(SQLite3 REQUIRED)\n")
endif()

install(TARGETS ${GERUEST_INSTALL_TARGETS}
    EXPORT GeruestTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES
    ${GERUEST_ROOT}/include/geruest/Core.hpp
    ${GERUEST_ROOT}/include/geruest/WebSocket.hpp
    ${GERUEST_ROOT}/include/geruest/Database.hpp
    ${GERUEST_ROOT}/include/geruest/Email.hpp
    ${GERUEST_ROOT}/include/geruest/Obfuscation.hpp
    ${GERUEST_ROOT}/include/geruest/Assets.hpp
    ${GERUEST_ROOT}/include/geruest/Geruest.hpp
    DESTINATION include/geruest
)

install(FILES ${GERUEST_ROOT}/src/Geruest.hpp DESTINATION include)

# Preserve src/ subtree paths (Geruest.hpp uses "data/...", "parser/...", etc.)
install(DIRECTORY ${GERUEST_ROOT}/src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
        PATTERN "unitTests" EXCLUDE)

install(FILES
    "${GERUEST_BINARY_DIR}/include/geruest/Version.hpp"
    "${GERUEST_BINARY_DIR}/include/geruest/BuildConfig.hpp"
    DESTINATION include/geruest
)

install(EXPORT GeruestTargets
    FILE GeruestTargets.cmake
    NAMESPACE Geruest::
    DESTINATION lib/cmake/Geruest
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${GERUEST_ROOT}/cmake/GeruestConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Geruest
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GeruestConfigVersion.cmake"
    DESTINATION lib/cmake/Geruest
)

# Optional: build unit tests from the same configure (uses targets defined above).
option(GERUEST_BUILD_TESTS "Build unit tests" OFF)
if(GERUEST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${GERUEST_ROOT}/src/unitTests ${CMAKE_BINARY_DIR}/unitTests)
endif()
