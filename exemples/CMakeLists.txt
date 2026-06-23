cmake_minimum_required(VERSION 3.11)
project(GeruestExamples VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Reuse the main library target (avoids drifting source lists).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/_geruest_core EXCLUDE_FROM_ALL)

set(GERUEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_subdirectory(minimal)
add_subdirectory(showcase)
