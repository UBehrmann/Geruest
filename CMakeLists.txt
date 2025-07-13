cmake_minimum_required(VERSION 3.10)
project(Geruest)

set(CMAKE_CXX_STANDARD 17)

set(PROJECT_NAME Geruest)

# Create the static library
add_library(
        ${PROJECT_NAME} STATIC
        src/Geruest.cpp
        src/handler/Handler.cpp
        src/db/SQLiteInterfaceSingleton.cpp
        src/parser/JSONParser.cpp
)

# Specify include directories
target_include_directories(
        ${PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

# Add the test executable
add_executable(
        exemple
        exemple/exemple.cpp
)

# Link the test executable with the library
target_link_libraries(
        exemple
        ${PROJECT_NAME}
        Threads::Threads
        SQLite::SQLite3
)

# Specify include directories for the test executable
target_include_directories(
        exemple PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
