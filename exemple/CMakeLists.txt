cmake_minimum_required(VERSION 3.10)
project(GeruestExample VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable static linking for MinGW
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Build the Geruest library first
add_library(Geruest STATIC
    ../src/Geruest.cpp
    ../src/auth/BasicAuth.cpp
    ../src/builders/ContentBuilder.cpp
    ../src/builders/CSSBuilder.cpp
    ../src/builders/HTMLBuilder.cpp
    ../src/builders/JSBuilder.cpp
    ../src/data/HTTPRequest.cpp
    ../src/data/HTTPResponse.cpp
    ../src/FileManagement/FileManagement.cpp
    ../src/handler/Handler.cpp
    ../src/parser/JSONParser.cpp
)

target_include_directories(Geruest PUBLIC
    ../src
)

target_link_libraries(Geruest
    Threads::Threads
)

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
