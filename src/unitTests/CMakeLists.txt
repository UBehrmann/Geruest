cmake_minimum_required(VERSION 3.28)

# Keep test Version.hpp in sync with GeruestLibrary (separate CMake project).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../CMakeLists.txt" GERUEST_ROOT_CMAKELISTS)
string(REGEX MATCH "project\\(GeruestLibrary VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${GERUEST_ROOT_CMAKELISTS}")
if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Could not parse Geruest library version from root CMakeLists.txt")
endif()
project(Geruest_Unit_Tests VERSION ${CMAKE_MATCH_1})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure version header (same as main project)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/geruest/Version.hpp"
)

# Fetch Google Test
# DOWNLOAD_EXTRACT_TIMESTAMP requires CMake 3.24+; this directory sets 3.28+.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

# Optional: Find CURL for email functionality
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL library found - Email functionality enabled in tests")
    set(GERUEST_HAS_CURL TRUE)
    add_compile_definitions(GERUEST_HAS_CURL=1)
else()
    message(STATUS "CURL library not found - Email functionality disabled in tests")
    set(GERUEST_HAS_CURL FALSE)
    add_compile_definitions(GERUEST_HAS_CURL=0)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost 1.75 REQUIRED COMPONENTS system)

option(GERUEST_ENABLE_POSTGRESQL "Enable PostgreSQL backend support in tests" OFF)
option(GERUEST_ENABLE_SQLITE "Enable SQLite backend support in tests" OFF)

set(GERUEST_HAS_LIBPQ FALSE)
if(GERUEST_ENABLE_POSTGRESQL)
    find_package(PostgreSQL QUIET)
    if(PostgreSQL_FOUND)
        set(GERUEST_HAS_LIBPQ TRUE)
        add_compile_definitions(GERUEST_HAS_LIBPQ=1)
    else()
        add_compile_definitions(GERUEST_HAS_LIBPQ=0)
    endif()
else()
    add_compile_definitions(GERUEST_HAS_LIBPQ=0)
endif()

set(GERUEST_HAS_SQLITE FALSE)
if(GERUEST_ENABLE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        set(GERUEST_HAS_SQLITE TRUE)
        add_compile_definitions(GERUEST_HAS_SQLITE=1)
    else()
        add_compile_definitions(GERUEST_HAS_SQLITE=0)
    endif()
else()
    add_compile_definitions(GERUEST_HAS_SQLITE=0)
endif()

foreach(_geruest_flag CURL LIBPQ SQLITE)
    if(GERUEST_HAS_${_geruest_flag})
        set(GERUEST_HAS_${_geruest_flag}_NUM 1)
    else()
        set(GERUEST_HAS_${_geruest_flag}_NUM 0)
    endif()
endforeach()
unset(_geruest_flag)
set(GERUEST_HAS_WEBP_NUM 0)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../geruest/BuildConfig.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/geruest/BuildConfig.hpp"
)

# Linux/Unix settings
set(PLATFORM_LIBS pthread Boost::system)

set(SERVERDATA_SOURCES
    ../data/DevAssetCache.cpp
    ../data/TextResponseCache.cpp
    ../data/GateRegistry.cpp
    ../data/LanguageConfig.cpp
    ../data/ObfuscationSettings.cpp
    ../data/RouteRegistry.cpp
    ../data/ServerData.cpp
    ../data/ServerMetrics.cpp
    ../data/WildcardMatch.cpp
)

set(ASSET_MERGER_SOURCES
    ../builders/AssetHtmlDiscovery.cpp
    ../builders/AssetMerger.cpp
)

# Collect all source files from the main Geruest library
set(GERUEST_SOURCES
    ../Geruest.cpp
    ../server/Config.cpp
    ../server/Socket.cpp
    ../server/Workers.cpp
    ../server/HttpSession.cpp
    ../server/WebSocket.cpp
    ../server/Status.cpp
    ../auth/BasicAuth.cpp
    ../config/ConfigLoader.cpp
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../data/MethodNotAllowed.cpp
    ../data/CorsConfig.cpp
    ${SERVERDATA_SOURCES}
    ../parser/JSONParser.cpp
    ../FileManagement/FileManagement.cpp
    ${ASSET_MERGER_SOURCES}
    ../builders/ContentBuilder.cpp
    ../builders/HTMLBuilder.cpp
    ../builders/CSSBuilder.cpp
    ../builders/JSBuilder.cpp
    ../builders/JSObfuscator.cpp
    ../builders/JSObfuscatorScope.cpp
    ../builders/WebPConverter.cpp
    ../handler/Handler.cpp
    ../handler/SyncGateExecutor.cpp
    ../handler/HttpFraming.cpp
    ../handler/ResponseWriter.cpp
    ../handler/StaticFileResolver.cpp
    ../handler/RouteDispatcher.cpp
    ../security/Security.cpp
)

# Conditionally add EmailSender if CURL is available
if(GERUEST_HAS_CURL)
    list(APPEND GERUEST_SOURCES ../email/EmailSender.cpp)
endif()

if(GERUEST_HAS_LIBPQ OR GERUEST_HAS_SQLITE)
    list(APPEND GERUEST_SOURCES
        ../database/DatabaseClient.cpp
        ../database/DbExecutor.cpp
    )
endif()

# Create individual test executables
# Note: No longer building a combined Geruest_Unit_Tests executable
# All tests are run via individual executables or ctest
add_executable(JSONParser_Tests
    JSONParser/JSONParser_tests.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
)
target_link_libraries(JSONParser_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(HTTPRequest_Tests
    HTTPRequest/HTTPRequest_tests.cpp
    ../data/HTTPRequest.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
)
target_link_libraries(HTTPRequest_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(HTTPResponse_Tests
    HTTPResponse/HTTPResponse_tests.cpp
    ../data/HTTPResponse.cpp
    ../data/HTTPRequest.cpp
    ../data/MethodNotAllowed.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
)
target_link_libraries(HTTPResponse_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(FileManagement_Tests FileManagement/FileManagement_tests.cpp ../FileManagement/FileManagement.cpp)
target_link_libraries(FileManagement_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(DevAssetCache_Tests
    DevAssetCache/DevAssetCache_tests.cpp
    ../data/DevAssetCache.cpp
)
target_link_libraries(DevAssetCache_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(TextResponseCache_Tests
    TextResponseCache/TextResponseCache_tests.cpp
    ../data/TextResponseCache.cpp
)
target_link_libraries(TextResponseCache_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(CorsConfig_Tests
    CorsConfig/CorsConfig_tests.cpp
    ../data/CorsConfig.cpp
    ../data/HTTPResponse.cpp
    ../data/HTTPRequest.cpp
    ../data/MethodNotAllowed.cpp
    ../data/WildcardMatch.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
)
target_link_libraries(CorsConfig_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ContentBuilder_Tests
    ContentBuilder/ContentBuilder_tests.cpp 
    ../auth/BasicAuth.cpp
    ${SERVERDATA_SOURCES}
    ${ASSET_MERGER_SOURCES}
    ../builders/ContentBuilder.cpp
    ../builders/HTMLBuilder.cpp
    ../builders/CSSBuilder.cpp
    ../builders/JSBuilder.cpp
    ../builders/JSObfuscator.cpp
    ../builders/JSObfuscatorScope.cpp
    ../builders/WebPConverter.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
    ../FileManagement/FileManagement.cpp
)
target_link_libraries(ContentBuilder_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(JSObfuscator_Tests 
    JSObfuscator/JSObfuscator_tests.cpp 
    ../builders/JSObfuscator.cpp
    ../builders/JSObfuscatorScope.cpp
)
target_link_libraries(JSObfuscator_Tests ${PLATFORM_LIBS} GTest::gtest_main)
# Optional syntax check: npm i acorn && js < tools/acorn_validate_stdin.js (repo root)

add_executable(BasicAuth_Tests 
    BasicAuth/BasicAuth_tests.cpp 
    ../auth/BasicAuth.cpp
)
target_link_libraries(BasicAuth_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ConfigLoader_Tests 
    ConfigLoader/ConfigLoader_tests.cpp 
    ../config/ConfigLoader.cpp
)
target_link_libraries(ConfigLoader_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(AssetMerger_Tests 
    AssetMerger/AssetMerger_tests.cpp 
    ${ASSET_MERGER_SOURCES}
    ../builders/WebPConverter.cpp
    ../FileManagement/FileManagement.cpp
)
target_link_libraries(AssetMerger_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(AssetHtmlDiscovery_Tests
    AssetHtmlDiscovery/AssetHtmlDiscovery_tests.cpp
    ../builders/AssetHtmlDiscovery.cpp
)
target_link_libraries(AssetHtmlDiscovery_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(Security_Tests
    Security/Security_tests.cpp
    ../security/Security.cpp
)
target_link_libraries(Security_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(WebSocketCodec_Tests
    WebSocket/WebSocketCodec_tests.cpp
    ../server/WebSocket.cpp
    ../data/HTTPRequest.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
)
target_link_libraries(WebSocketCodec_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(WebSocketHandshake_Tests
    WebSocket/WebSocketHandshake_tests.cpp
    ../server/WebSocket.cpp
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../data/MethodNotAllowed.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
)
target_link_libraries(WebSocketHandshake_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(WebSocketServerData_Tests
    WebSocket/WebSocketServerData_tests.cpp
    ${SERVERDATA_SOURCES}
    ../data/HTTPRequest.cpp
    ../server/WebSocket.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(WebSocketServerData_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(WebSocketIntegration_Tests
    WebSocket/WebSocketIntegration_tests.cpp
    ${GERUEST_SOURCES}
)
target_link_libraries(WebSocketIntegration_Tests ${PLATFORM_LIBS} GTest::gtest_main)
if(GERUEST_HAS_CURL)
    target_link_libraries(WebSocketIntegration_Tests CURL::libcurl)
endif()
if(GERUEST_HAS_SQLITE)
    target_link_libraries(WebSocketIntegration_Tests SQLite::SQLite3)
endif()
if(GERUEST_HAS_LIBPQ)
    target_link_libraries(WebSocketIntegration_Tests PostgreSQL::PostgreSQL)
endif()

add_executable(ServerDataMetricsPersistence_Tests
    ServerData/ServerDataMetricsPersistence_tests.cpp
    ${SERVERDATA_SOURCES}
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(ServerDataMetricsPersistence_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ServerDataRedirectLanguage_Tests
    ServerData/ServerDataRedirectLanguage_tests.cpp
    ${SERVERDATA_SOURCES}
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(ServerDataRedirectLanguage_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(StaticFileResolver_Tests
    Handler/StaticFileResolver_tests.cpp
    ../handler/StaticFileResolver.cpp
    ${SERVERDATA_SOURCES}
    ../data/HTTPRequest.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(StaticFileResolver_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ServerDataPageGate_Tests
    ServerData/ServerDataPageGate_tests.cpp
    ${SERVERDATA_SOURCES}
    ../data/HTTPRequest.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(ServerDataPageGate_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ServerDataRouteGate_Tests
    ServerData/ServerDataRouteGate_tests.cpp
    ${SERVERDATA_SOURCES}
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(ServerDataRouteGate_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(GateEvaluation_Tests
    GateEvaluation/GateEvaluation_tests.cpp
    ../handler/SyncGateExecutor.cpp
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../parser/JSONParser.cpp
    ../security/Security.cpp
)
target_link_libraries(GateEvaluation_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(RouteRegistry_Tests
    RouteRegistry/RouteRegistry_tests.cpp
    ${SERVERDATA_SOURCES}
    ../data/HTTPRequest.cpp
    ../data/HTTPResponse.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(RouteRegistry_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(ServerDataMergedAsset_Tests
    ServerData/ServerDataMergedAsset_tests.cpp
    ${SERVERDATA_SOURCES}
    ../data/HTTPRequest.cpp
    ../parser/JSONParser.cpp
    ../auth/BasicAuth.cpp
    ../security/Security.cpp
    ${ASSET_MERGER_SOURCES}
)
target_link_libraries(ServerDataMergedAsset_Tests ${PLATFORM_LIBS} GTest::gtest_main)

add_executable(GeruestLifecycle_Tests
    Geruest/GeruestLifecycle_tests.cpp
    ${GERUEST_SOURCES}
)
target_link_libraries(GeruestLifecycle_Tests ${PLATFORM_LIBS} GTest::gtest_main)
if(GERUEST_HAS_CURL)
    target_link_libraries(GeruestLifecycle_Tests CURL::libcurl)
endif()
if(GERUEST_HAS_SQLITE)
    target_link_libraries(GeruestLifecycle_Tests SQLite::SQLite3)
endif()
if(GERUEST_HAS_LIBPQ)
    target_link_libraries(GeruestLifecycle_Tests PostgreSQL::PostgreSQL)
endif()

if(GERUEST_HAS_SQLITE)
    add_executable(DatabaseSqlite_Tests
        Database/DatabaseSqlite_tests.cpp
        ../database/DatabaseClient.cpp
        ../database/DbExecutor.cpp
        ../parser/JSONParser.cpp
        ../security/Security.cpp
    )
    # Work around GCC 13 internal compiler error in coroutine lowering for this test TU.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
        set_source_files_properties(Database/DatabaseSqlite_tests.cpp PROPERTIES COMPILE_OPTIONS "-O0")
    endif()
    target_link_libraries(DatabaseSqlite_Tests ${PLATFORM_LIBS} GTest::gtest_main SQLite::SQLite3)
    if(GERUEST_HAS_LIBPQ)
        target_link_libraries(DatabaseSqlite_Tests PostgreSQL::PostgreSQL)
    endif()
endif()

if(GERUEST_HAS_LIBPQ)
    add_executable(DatabasePostgres_Tests
        Database/DatabasePostgres_tests.cpp
        ../database/DatabaseClient.cpp
        ../database/DbExecutor.cpp
        ../parser/JSONParser.cpp
        ../security/Security.cpp
    )
    # Work around GCC 13 internal compiler error in coroutine lowering for this test TU.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
        set_source_files_properties(Database/DatabasePostgres_tests.cpp PROPERTIES COMPILE_OPTIONS "-O0")
    endif()
    target_link_libraries(DatabasePostgres_Tests ${PLATFORM_LIBS} GTest::gtest_main PostgreSQL::PostgreSQL)
    if(GERUEST_HAS_SQLITE)
        target_link_libraries(DatabasePostgres_Tests SQLite::SQLite3)
    endif()
endif()

# Add tests to CTest using Google Test discovery
include(GoogleTest)
gtest_discover_tests(JSONParser_Tests)
gtest_discover_tests(HTTPRequest_Tests)
gtest_discover_tests(HTTPResponse_Tests)
gtest_discover_tests(FileManagement_Tests)
gtest_discover_tests(DevAssetCache_Tests)
gtest_discover_tests(TextResponseCache_Tests)
gtest_discover_tests(CorsConfig_Tests)
gtest_discover_tests(ContentBuilder_Tests)
gtest_discover_tests(JSObfuscator_Tests)
gtest_discover_tests(BasicAuth_Tests)
gtest_discover_tests(ConfigLoader_Tests)
gtest_discover_tests(AssetMerger_Tests)
gtest_discover_tests(AssetHtmlDiscovery_Tests)
gtest_discover_tests(Security_Tests)
gtest_discover_tests(WebSocketCodec_Tests)
gtest_discover_tests(WebSocketHandshake_Tests)
gtest_discover_tests(WebSocketServerData_Tests)
gtest_discover_tests(WebSocketIntegration_Tests)
gtest_discover_tests(ServerDataMetricsPersistence_Tests)
gtest_discover_tests(ServerDataRedirectLanguage_Tests)
gtest_discover_tests(StaticFileResolver_Tests)
gtest_discover_tests(ServerDataPageGate_Tests)
gtest_discover_tests(ServerDataRouteGate_Tests)
gtest_discover_tests(GateEvaluation_Tests)
gtest_discover_tests(RouteRegistry_Tests)
gtest_discover_tests(ServerDataMergedAsset_Tests)
gtest_discover_tests(GeruestLifecycle_Tests)
if(GERUEST_HAS_SQLITE)
    gtest_discover_tests(DatabaseSqlite_Tests)
endif()
if(GERUEST_HAS_LIBPQ)
    gtest_discover_tests(DatabasePostgres_Tests)
endif()

# Custom target to run all tests
if(GERUEST_HAS_LIBPQ)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/run_tests_pg.sh" [=[#!/usr/bin/env bash
set -e
docker rm -f geruest_test_pg 2>/dev/null || true
docker run -d --name geruest_test_pg \
  -e POSTGRES_USER=geruest -e POSTGRES_PASSWORD=geruest -e POSTGRES_DB=geruest_test \
  -p 5432:5432 postgres:16
echo 'Waiting for PostgreSQL...'
for i in $(seq 1 30); do
  docker exec geruest_test_pg pg_isready -U geruest -q 2>/dev/null && break || sleep 1
done
export POSTGRES_HOST=localhost POSTGRES_PORT=5432
export POSTGRES_DB=geruest_test POSTGRES_USER=geruest POSTGRES_PASSWORD=geruest
rc=0
"$1" --output-on-failure --verbose || rc=$?
docker rm -f geruest_test_pg
exit $rc
]=])
    add_custom_target(run_tests
        COMMAND bash "${CMAKE_CURRENT_BINARY_DIR}/run_tests_pg.sh" "${CMAKE_CTEST_COMMAND}"
        DEPENDS JSONParser_Tests HTTPRequest_Tests HTTPResponse_Tests FileManagement_Tests ContentBuilder_Tests JSObfuscator_Tests BasicAuth_Tests ConfigLoader_Tests AssetMerger_Tests Security_Tests WebSocketCodec_Tests WebSocketHandshake_Tests WebSocketServerData_Tests WebSocketIntegration_Tests ServerDataMetricsPersistence_Tests ServerDataRedirectLanguage_Tests StaticFileResolver_Tests ServerDataPageGate_Tests ServerDataRouteGate_Tests GateEvaluation_Tests RouteRegistry_Tests GeruestLifecycle_Tests DatabasePostgres_Tests
        COMMENT "Running all unit tests via CTest (with PostgreSQL container)"
    )
else()
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
        DEPENDS JSONParser_Tests HTTPRequest_Tests HTTPResponse_Tests FileManagement_Tests ContentBuilder_Tests JSObfuscator_Tests BasicAuth_Tests ConfigLoader_Tests AssetMerger_Tests Security_Tests WebSocketCodec_Tests WebSocketHandshake_Tests WebSocketServerData_Tests WebSocketIntegration_Tests ServerDataMetricsPersistence_Tests ServerDataRedirectLanguage_Tests StaticFileResolver_Tests ServerDataPageGate_Tests ServerDataRouteGate_Tests GateEvaluation_Tests RouteRegistry_Tests GeruestLifecycle_Tests
        COMMENT "Running all unit tests via CTest"
    )
endif()

if(GERUEST_HAS_SQLITE)
    add_dependencies(run_tests DatabaseSqlite_Tests)
endif()

# Copy test data files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/JSONParser/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/JSONParser
     FILES_MATCHING PATTERN "*.json")
