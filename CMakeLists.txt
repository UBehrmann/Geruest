cmake_minimum_required(VERSION 3.10)
project(GeruestLibrary VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable static linking for MinGW
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
endif()

# Output static libs to /lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Required packages
find_package(Threads REQUIRED)

# Create the static library
add_library(Geruest STATIC
    src/Geruest.cpp
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
