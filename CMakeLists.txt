cmake_minimum_required(VERSION 3.10)
project(Geruest)

# Create the static library
add_library(PROJECT_NAME STATIC
    src/geruest.cpp
)

# Specify include directories
target_include_directories(PROJECT_NAME PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
