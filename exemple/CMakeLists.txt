cmake_minimum_required(VERSION 3.10)
project(GeruestExample VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific: Prevent min/max macros that conflict with std::min/std::max
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Enable static linking for MinGW
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost 1.75 REQUIRED COMPONENTS system)

# Find CURL (optional for email functionality)
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL library found - Email functionality enabled")
    set(GERUEST_HAS_CURL 1)
else()
    message(STATUS "CURL library not found - Email functionality disabled")
    set(GERUEST_HAS_CURL 0)
endif()

# Check for WebP library (optional)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(WEBP QUIET libwebp)
endif()

if(WEBP_FOUND)
    message(STATUS "WebP library found - WebP conversion enabled")
    set(GERUEST_HAS_WEBP 1)
else()
    # Try finding without pkg-config
    find_library(WEBP_LIBRARY NAMES webp)
    find_path(WEBP_INCLUDE_DIR webp/encode.h)
    if(WEBP_LIBRARY AND WEBP_INCLUDE_DIR)
        message(STATUS "WebP library found - WebP conversion enabled")
        set(GERUEST_HAS_WEBP 1)
        set(WEBP_LIBRARIES ${WEBP_LIBRARY})
        set(WEBP_INCLUDE_DIRS ${WEBP_INCLUDE_DIR})
    else()
        message(STATUS "WebP library not found - WebP conversion disabled")
        set(GERUEST_HAS_WEBP 0)
    endif()
endif()

# Build the Geruest library first
set(GERUEST_SOURCES
    ../src/Geruest.cpp
    ../src/server/Config.cpp
    ../src/server/Socket.cpp
    ../src/server/Workers.cpp
    ../src/server/HttpSession.cpp
    ../src/server/WebSocket.cpp
    ../src/server/Status.cpp
    ../src/auth/BasicAuth.cpp
    ../src/builders/AssetMerger.cpp
    ../src/builders/ContentBuilder.cpp
    ../src/builders/CSSBuilder.cpp
    ../src/builders/HTMLBuilder.cpp
    ../src/builders/JSBuilder.cpp
    ../src/builders/JSObfuscator.cpp
    ../src/builders/JSObfuscatorScope.cpp
    ../src/builders/WebPConverter.cpp
    ../src/config/ConfigLoader.cpp
    ../src/data/HTTPRequest.cpp
    ../src/data/HTTPResponse.cpp
    ../src/data/MethodNotAllowed.cpp
    ../src/data/ServerData.cpp
    ../src/FileManagement/FileManagement.cpp
    ../src/handler/Handler.cpp
    ../src/parser/JSONParser.cpp
    ../src/security/Security.cpp
)

# Conditionally add EmailSender if CURL is available
if(GERUEST_HAS_CURL)
    list(APPEND GERUEST_SOURCES ../src/email/EmailSender.cpp)
endif()

add_library(Geruest STATIC ${GERUEST_SOURCES})

target_include_directories(Geruest PUBLIC
    ../src
)

# Add WebP compile definition and link library
target_compile_definitions(Geruest PUBLIC GERUEST_HAS_WEBP=${GERUEST_HAS_WEBP})
if(GERUEST_HAS_WEBP)
    target_include_directories(Geruest PRIVATE ${WEBP_INCLUDE_DIRS})
    target_link_libraries(Geruest ${WEBP_LIBRARIES})
endif()

# Add CURL compile definition and link library
target_compile_definitions(Geruest PUBLIC GERUEST_HAS_CURL=${GERUEST_HAS_CURL})

target_link_libraries(Geruest Threads::Threads Boost::system)

if(GERUEST_HAS_CURL)
    target_link_libraries(Geruest CURL::libcurl)
endif()

# Link Windows socket library on Windows
if(WIN32)
    target_link_libraries(Geruest ws2_32)
endif()

# Example executable
add_executable(exemple
    exemple.cpp
)

target_include_directories(exemple PRIVATE
    ../src
)

target_link_libraries(exemple Geruest)

# Optional: Strip on Linux release
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(exemple PRIVATE "-s")
    endif()
endif()

# Copy website folder to build directory
add_custom_command(TARGET exemple POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/website
    $<TARGET_FILE_DIR:exemple>/website
    COMMENT "Copying website folder to build directory"
)
