# Geruest library targets (included from root CMakeLists.txt).
# Expects: GERUEST_ROOT, GERUEST_BINARY_DIR, GERUEST_HAS_*, GERUEST_ENABLE_* options.

if(TARGET GeruestCore)
    return()
endif()

if(NOT GERUEST_ROOT)
    message(FATAL_ERROR "GERUEST_ROOT must be set before including GeruestModules.cmake")
endif()

function(_geruest_public_includes target)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${GERUEST_ROOT}/src>
        $<BUILD_INTERFACE:${GERUEST_ROOT}/include>
        $<BUILD_INTERFACE:${GERUEST_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endfunction()

set(_GERUEST_CORE_SOURCES
    ${GERUEST_ROOT}/src/Geruest.cpp
    ${GERUEST_ROOT}/src/server/Config.cpp
    ${GERUEST_ROOT}/src/server/Socket.cpp
    ${GERUEST_ROOT}/src/server/Workers.cpp
    ${GERUEST_ROOT}/src/server/HttpSession.cpp
    ${GERUEST_ROOT}/src/server/Status.cpp
    ${GERUEST_ROOT}/src/auth/BasicAuth.cpp
    ${GERUEST_ROOT}/src/config/ConfigLoader.cpp
    ${GERUEST_ROOT}/src/data/HTTPRequest.cpp
    ${GERUEST_ROOT}/src/data/HTTPResponse.cpp
    ${GERUEST_ROOT}/src/data/MethodNotAllowed.cpp
    ${GERUEST_ROOT}/src/data/GateRegistry.cpp
    ${GERUEST_ROOT}/src/data/LanguageConfig.cpp
    ${GERUEST_ROOT}/src/data/RouteRegistry.cpp
    ${GERUEST_ROOT}/src/data/CorsConfig.cpp
    ${GERUEST_ROOT}/src/data/ServerData.cpp
    ${GERUEST_ROOT}/src/data/TextResponseCache.cpp
    ${GERUEST_ROOT}/src/data/ServerMetrics.cpp
    ${GERUEST_ROOT}/src/data/WildcardMatch.cpp
    ${GERUEST_ROOT}/src/data/DevAssetCache.cpp
    ${GERUEST_ROOT}/src/FileManagement/FileManagement.cpp
    ${GERUEST_ROOT}/src/handler/Handler.cpp
    ${GERUEST_ROOT}/src/handler/SyncGateExecutor.cpp
    ${GERUEST_ROOT}/src/handler/HttpFraming.cpp
    ${GERUEST_ROOT}/src/handler/ResponseWriter.cpp
    ${GERUEST_ROOT}/src/handler/StaticFileResolver.cpp
    ${GERUEST_ROOT}/src/handler/RouteDispatcher.cpp
    ${GERUEST_ROOT}/src/modules/ModuleHooks.cpp
    ${GERUEST_ROOT}/src/obfuscation/ObfuscationSettings.cpp
    ${GERUEST_ROOT}/src/parser/JSONParser.cpp
    ${GERUEST_ROOT}/src/security/Security.cpp
)

add_library(GeruestCore STATIC ${_GERUEST_CORE_SOURCES})
add_library(Geruest::Core ALIAS GeruestCore)
_geruest_public_includes(GeruestCore)
target_link_libraries(GeruestCore PUBLIC Threads::Threads Boost::system)

add_library(GeruestObfuscation STATIC
    ${GERUEST_ROOT}/src/builders/JSObfuscator.cpp
    ${GERUEST_ROOT}/src/builders/JSObfuscatorScope.cpp
)
add_library(Geruest::Obfuscation ALIAS GeruestObfuscation)
_geruest_public_includes(GeruestObfuscation)
target_link_libraries(GeruestObfuscation PUBLIC GeruestCore)

add_library(GeruestAssets STATIC
    ${GERUEST_ROOT}/src/assets/AssetsModule.cpp
    ${GERUEST_ROOT}/src/assets/MergedAssetResolver.cpp
    ${GERUEST_ROOT}/src/builders/AssetHtmlDiscovery.cpp
    ${GERUEST_ROOT}/src/builders/AssetMerger.cpp
    ${GERUEST_ROOT}/src/builders/ContentBuilder.cpp
    ${GERUEST_ROOT}/src/builders/CSSBuilder.cpp
    ${GERUEST_ROOT}/src/builders/HTMLBuilder.cpp
    ${GERUEST_ROOT}/src/builders/JSBuilder.cpp
    ${GERUEST_ROOT}/src/builders/WebPConverter.cpp
)
add_library(Geruest::Assets ALIAS GeruestAssets)
_geruest_public_includes(GeruestAssets)
target_link_libraries(GeruestAssets PUBLIC GeruestCore GeruestObfuscation)
if(GERUEST_HAS_WEBP)
    if(WEBP_LINK_TARGET)
        target_link_libraries(GeruestAssets PRIVATE ${WEBP_LINK_TARGET})
    else()
        target_include_directories(GeruestAssets PRIVATE ${LIBWEBP_INCLUDE_DIRS})
        target_link_libraries(GeruestAssets PRIVATE ${LIBWEBP_LIBRARIES})
        target_link_directories(GeruestAssets PRIVATE ${LIBWEBP_LIBRARY_DIRS})
    endif()
endif()

if(GERUEST_ENABLE_WEBSOCKET)
    add_library(GeruestWebSocket STATIC
        ${GERUEST_ROOT}/src/server/WebSocket.cpp
        ${GERUEST_ROOT}/src/websocket/WebSocketUpgrade.cpp
        ${GERUEST_ROOT}/src/websocket/GeruestWebSocketApi.cpp
    )
    add_library(Geruest::WebSocket ALIAS GeruestWebSocket)
    _geruest_public_includes(GeruestWebSocket)
    target_link_libraries(GeruestWebSocket PUBLIC GeruestCore)
endif()

if(GERUEST_HAS_LIBPQ OR GERUEST_HAS_SQLITE)
    add_library(GeruestDatabase STATIC
        ${GERUEST_ROOT}/src/database/DatabaseClient.cpp
        ${GERUEST_ROOT}/src/database/DbExecutor.cpp
    )
    add_library(Geruest::Database ALIAS GeruestDatabase)
    _geruest_public_includes(GeruestDatabase)
    target_link_libraries(GeruestDatabase PUBLIC GeruestCore)
    if(GERUEST_HAS_LIBPQ)
        target_link_libraries(GeruestDatabase PUBLIC PostgreSQL::PostgreSQL)
    endif()
    if(GERUEST_HAS_SQLITE)
        target_link_libraries(GeruestDatabase PUBLIC SQLite::SQLite3)
    endif()
endif()

if(GERUEST_HAS_CURL AND GERUEST_ENABLE_EMAIL)
    add_library(GeruestEmail STATIC
        ${GERUEST_ROOT}/src/email/EmailSender.cpp
        ${GERUEST_ROOT}/src/email/EmailConfig.cpp
        ${GERUEST_ROOT}/src/email/GeruestEmailApi.cpp
    )
    add_library(Geruest::Email ALIAS GeruestEmail)
    _geruest_public_includes(GeruestEmail)
    target_link_libraries(GeruestEmail PUBLIC GeruestCore CURL::libcurl)
endif()

# Umbrella INTERFACE target (backward compatible Geruest::Geruest)
add_library(Geruest INTERFACE)
add_library(Geruest::Geruest ALIAS Geruest)

set(_GERUEST_UMBRELLA_LIBS GeruestCore)
if(GERUEST_ENABLE_OBFUSCATION)
    list(APPEND _GERUEST_UMBRELLA_LIBS GeruestObfuscation)
endif()
if(GERUEST_ENABLE_ASSETS)
    list(APPEND _GERUEST_UMBRELLA_LIBS GeruestAssets)
endif()
if(GERUEST_ENABLE_WEBSOCKET)
    list(APPEND _GERUEST_UMBRELLA_LIBS GeruestWebSocket)
endif()
if(GERUEST_HAS_LIBPQ OR GERUEST_HAS_SQLITE)
    list(APPEND _GERUEST_UMBRELLA_LIBS GeruestDatabase)
endif()
if(GERUEST_HAS_CURL AND GERUEST_ENABLE_EMAIL)
    list(APPEND _GERUEST_UMBRELLA_LIBS GeruestEmail)
endif()
list(APPEND _GERUEST_UMBRELLA_LIBS GeruestCore)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    target_link_libraries(Geruest INTERFACE "$<LINK_GROUP:RESCAN,${_GERUEST_UMBRELLA_LIBS}>")
else()
    target_link_libraries(Geruest INTERFACE ${_GERUEST_UMBRELLA_LIBS})
    target_link_libraries(Geruest INTERFACE GeruestCore)
endif()

set(GERUEST_INSTALL_TARGETS GeruestCore GeruestObfuscation GeruestAssets Geruest)
if(GERUEST_ENABLE_WEBSOCKET)
    list(APPEND GERUEST_INSTALL_TARGETS GeruestWebSocket)
endif()
if(GERUEST_HAS_LIBPQ OR GERUEST_HAS_SQLITE)
    list(APPEND GERUEST_INSTALL_TARGETS GeruestDatabase)
endif()
if(GERUEST_HAS_CURL AND GERUEST_ENABLE_EMAIL)
    list(APPEND GERUEST_INSTALL_TARGETS GeruestEmail)
endif()
