cmake_minimum_required(VERSION 3.10)
project(Geruest VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output static libs to /lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Threads REQUIRED)

# Static library
add_library(Geruest STATIC
    src/Geruest.cpp
    src/handler/Handler.cpp
    src/parser/JSONParser.cpp
)

target_include_directories(Geruest PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(Geruest
    Threads::Threads
)

# Optional: Strip on Linux release
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(Geruest PRIVATE "-s")
    endif()
endif()

# Example executable
add_executable(exemple
    exemple/exemple.cpp
)

target_include_directories(exemple PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(exemple Geruest)
