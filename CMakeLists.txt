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

# Find required packages
find_package(Threads REQUIRED)

# Static library
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

target_include_directories(Geruest PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(Geruest
    Threads::Threads
)

# Link Windows socket library on Windows
if(WIN32)
    target_link_libraries(Geruest ws2_32)
endif()

# Optional: Strip on Linux release
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(Geruest PRIVATE "-s")
    endif()
endif()

# Installation rules
install(TARGETS Geruest
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY src/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)
