/**
 * @file Geruest.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Core server class — constructor, destructor, and short delegating methods.
 *        Complex implementations live in dedicated files:
 *          GeruestConfig.cpp   — loadConfig()
 *          GeruestSocket.cpp   — init(), start(), stop(), isRunning()
 *          GeruestWorkers.cpp  — thread pool
 *          GeruestStatus.cpp   — /status endpoint and system metrics
 */

#include "Geruest.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

#include "geruest/Version.hpp"
#include "modules/ModuleHooks.hpp"
#if GERUEST_ENABLE_ASSETS
#include "assets/AssetsModule.hpp"
#endif
#if GERUEST_ENABLE_WEBSOCKET
#include "websocket/WebSocketUpgrade.hpp"
#endif
#if GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
#include "email/EmailConfig.hpp"
#endif

namespace geruest {

// ========== Constructor / Destructor ==========

Geruest::Geruest() {
    // Disable buffering so log lines are visible even if the process is killed
    // by the OOM killer (SIGKILL leaves no time for buffer flushing).
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    modules::bindServerData(&serverData);
#if GERUEST_ENABLE_ASSETS
    (void)assets::assetsModuleLinkAnchor();
    assets::ensureAssetsModuleRegistered();
#endif
#if GERUEST_ENABLE_WEBSOCKET
    websocket::ensureWebSocketModuleRegistered();
#endif
#if GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
    (void)email::emailModuleLinkAnchor();
    modules::registerEmailConfigApplier(email::applyFromConfigLoader);
#endif

    std::cout << "Geruest Framework v" << getVersion() << std::endl;

}

Geruest::~Geruest() {
    running.store(false, std::memory_order_relaxed);
    if (_statusPersistenceThread.joinable()) {
        _statusPersistenceThread.join();
    }
    stopWorkers();

    sendToLogger("Server closed.");
}

// ========== Logging ==========

void Geruest::sendToLogger(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Info)) std::cout << message << std::endl;
}

void Geruest::sendToLoggerError(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Error)) std::cerr << "Error: " << message << std::endl;
}

void Geruest::setLogLevel(LogLevel level) {
    serverData.setLogLevel(level);
    _configFlags.logLevelSet = true;

    if (serverData.shouldLog(LogLevel::Info)) {
        std::string levelStr;
        switch (level) {
            case LogLevel::None:    levelStr = "None";    break;
            case LogLevel::Error:   levelStr = "Error";   break;
            case LogLevel::Warning: levelStr = "Warning"; break;
            case LogLevel::Info:    levelStr = "Info";    break;
            case LogLevel::Debug:   levelStr = "Debug";   break;
        }
        sendToLogger("Log level set to: " + levelStr);
    }
}

LogLevel Geruest::getLogLevel() const { return serverData.getLogLevel(); }

// ========== Server Configuration ==========

void Geruest::setPort(int _port) { port = _port; _configFlags.portSet = true; }

void Geruest::setHostname(const std::string& hostname) { hostname_ = hostname; _configFlags.hostnameSet = true; }

void Geruest::setBindAddress(const std::string& address) {
    bindAddress_ = address;
    _configFlags.bindAddressSet = true;
}

void Geruest::setStatusPersistencePath(std::string path) { _statusPersistencePath = std::move(path); }

void Geruest::addRoute(const std::string& path, RouteHandler routeHandler) {
    serverData.addRoute(path, std::move(routeHandler));
}

void Geruest::addRoute(const std::string& path, AsyncRouteHandler routeHandler) {
    serverData.addRoute(path, std::move(routeHandler));
}

void Geruest::addRoute(const std::string& path, RouteHandler handler, RouteGateHandler gate) {
    if (path.empty() || !handler || !gate) {
        sendToLoggerError("Failed to add route with gate (path/handler/gate invalid): " + path);
        return;
    }
    serverData.addRoute(path, std::move(handler));
    serverData.addRouteGate(path, std::move(gate));
    sendToLogger("Added route with gate: " + path);
}

void Geruest::addRoute(const std::string& path, RouteHandler handler, AsyncRouteGateHandler gate) {
    if (path.empty() || !handler || !gate) {
        sendToLoggerError("Failed to add route with async gate (path/handler/gate invalid): " + path);
        return;
    }
    serverData.addRoute(path, std::move(handler));
    serverData.addAsyncRouteGate(path, std::move(gate));
    sendToLogger("Added route with async gate: " + path);
}

void Geruest::addRoute(const std::string& path, AsyncRouteHandler handler, AsyncRouteGateHandler gate) {
    if (path.empty() || !handler || !gate) {
        sendToLoggerError("Failed to add async route with async gate (path/handler/gate invalid): " + path);
        return;
    }
    serverData.addRoute(path, std::move(handler));
    serverData.addAsyncRouteGate(path, std::move(gate));
    sendToLogger("Added async route with async gate: " + path);
}

void Geruest::setDatabaseBackend(DatabaseBackend backend) {
    _databaseBackend = backend;
    _configFlags.databaseBackendSet = true;
}

void Geruest::setDatabasePoolSize(size_t size) {
    if (size == 0) {
        sendToLoggerError("Database pool size must be at least 1");
        return;
    }
    _dbCommonConfig.poolSize = size;
    _configFlags.databasePoolSizeSet = true;
}

void Geruest::setSqliteExecutorThreadCount(size_t count) {
    if (count == 0) {
        sendToLoggerError("SQLite executor thread count must be at least 1");
        return;
    }
    _dbCommonConfig.sqliteExecutorThreads = count;
    _configFlags.sqliteExecutorThreadsSet = true;
}

#if GERUEST_HAS_LIBPQ
void Geruest::configurePostgres(const db::PostgresConfig& config) {
    _postgresConfig = config;
    _configFlags.postgresConfigSet = true;
}
#endif

#if GERUEST_HAS_SQLITE
void Geruest::configureSqlite(const db::SqliteConfig& config) {
    _sqliteConfig = config;
    _configFlags.sqliteConfigSet = true;
}
#endif

void Geruest::addRedirect(const std::string& from, const std::string& to, int status) {
    if (serverData.addRedirect(from, to, status)) {
        sendToLogger("Added redirect: " + from + " -> " + to + " (" + std::to_string(status) + ")");
    } else {
        sendToLoggerError("Skipped redirect (invalid or loop detected): " + from + " -> " + to);
    }
}

void Geruest::addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status) {
    const size_t addedCount = serverData.addRedirects(redirects, status);
    sendToLogger("Added redirect map entries: " + std::to_string(addedCount) + "/" +
                 std::to_string(redirects.size()) + " (" + std::to_string(status) + ")");
}

void Geruest::addRoot(const std::string& root) { serverData.setRoot(root); }

void Geruest::set404(const std::string& path) {
    serverData.setNotFoundPage(path);
    sendToLogger("Custom 404 page set to: " + path);
}

void Geruest::setAvailableLanguages(const std::vector<std::string>& languages) {
    serverData.setAvailableLanguages(languages);
    if (!languages.empty()) {
        sendToLogger("Available languages: " + std::to_string(languages.size()) + ", default: " + languages[0]);
    } else {
        sendToLogger("No languages configured - language routing disabled");
    }
}

// ========== Asset / WebP / Obfuscation ==========

void Geruest::setMergeAssets(bool enabled) {
    serverData.setMergeAssets(enabled);
    _configFlags.mergeAssetsSet = true;
    sendToLogger(std::string("Asset merging ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::enableMergeAssets() { setMergeAssets(true); }

void Geruest::setWebPConversion(bool enabled) {
#if GERUEST_HAS_WEBP
    serverData.setWebPConversion(enabled);
    _configFlags.webpConversionSet = true;
    sendToLogger(std::string("WebP conversion ") + (enabled ? "enabled" : "disabled"));
#else
    if (enabled) {
        sendToLoggerError("WebP conversion cannot be enabled - library not available (GERUEST_HAS_WEBP=0)");
        sendToLoggerError("Install libwebp-dev (apt) or webp (vcpkg) and rebuild to enable this feature");
    }
    serverData.setWebPConversion(false);
#endif
}

void Geruest::enableWebPConversion() {
#if GERUEST_HAS_WEBP
    serverData.enableWebPConversion();
    _configFlags.webpConversionSet = true;
    sendToLogger("WebP conversion enabled");
#else
    sendToLoggerError("WebP conversion cannot be enabled - library not available (GERUEST_HAS_WEBP=0)");
    sendToLoggerError("Install libwebp-dev (apt) or webp (vcpkg) and rebuild to enable this feature");
    serverData.setWebPConversion(false);
#endif
}

void Geruest::setWebPQuality(float quality) {
#if GERUEST_HAS_WEBP
    serverData.setWebPQuality(quality);
    _configFlags.webpQualitySet = true;
    std::ostringstream oss;
    oss << "WebP quality set to " << quality << "%";
    sendToLogger(oss.str());
#else
    sendToLoggerError("WebP quality cannot be set - WebP conversion not available (GERUEST_HAS_WEBP=0)");
#endif
}

void Geruest::setWebPMaxDimension(int maxDimension) {
#if GERUEST_HAS_WEBP
    modules::setWebpMaxDimension(maxDimension);
    if (maxDimension > 0) {
        sendToLogger("WebP max dimension set to " + std::to_string(maxDimension) + "px");
    } else {
        sendToLogger("WebP automatic resizing disabled");
    }
#else
    (void)maxDimension;
    sendToLoggerError("WebP max dimension cannot be set - WebP conversion not available (GERUEST_HAS_WEBP=0)");
#endif
}

void Geruest::setObfuscationLevel(unsigned int level) {
    serverData.setObfuscationLevel(level);
    sendToLogger(level > 0 ? "JS obfuscation enabled (level " + std::to_string(level) + ")"
                           : "JS obfuscation disabled");
}

void Geruest::setObfuscationCacheExpiry(int days) {
    serverData.setObfuscationCacheExpiry(days);
    sendToLogger("Obfuscation cache expiry set to " + std::to_string(days) + " days");
}

void Geruest::addObfuscationExclusion(const std::string& filename) {
    serverData.addObfuscationExclusion(filename);
    sendToLogger("Added obfuscation exclusion: " + filename);
}

void Geruest::addObfuscationPreserveIdent(const std::string& name) {
    serverData.addObfuscationPreserveIdent(name);
}

void Geruest::addObfuscationExternGlobal(const std::string& name) {
    serverData.addObfuscationExternGlobal(name);
}

bool Geruest::loadObfuscationExternsFile(const std::string& pathRelativeToRoot) {
    namespace fs = std::filesystem;
    fs::path p = fs::path(serverData.getRoot()) / pathRelativeToRoot;
    std::ifstream f(p);
    if (!f) {
        sendToLoggerError("Obfuscation externs file not readable: " + p.string());
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    serverData.loadObfuscationExternsFromText(ss.str());
    sendToLogger("Loaded obfuscation externs from: " + p.string());
    return true;
}

void Geruest::setObfuscationStrictUndefined(bool enabled) {
    serverData.setObfuscationStrictUndefined(enabled);
}

void Geruest::setObfuscationEmitGlobalThisAssignments(bool enabled) {
    serverData.setObfuscationEmitGlobalThisAssignments(enabled);
}

void Geruest::setObfuscationValidateWithAcorn(bool enabled) {
    serverData.setObfuscationValidateWithAcorn(enabled);
}

void Geruest::setObfuscationAutoBracketKeys(bool enabled) {
    serverData.setObfuscationAutoBracketKeys(enabled);
}

void Geruest::enableDevMode() {
    serverData.enableDevMode();
    _configFlags.devModeSet = true;
    sendToLogger("Development mode enabled: verbose logging, no file caching, comments preserved");
}

// ========== Thread Pool Configuration ==========

void Geruest::setWorkerThreadCount(size_t count) {
    if (running.load(std::memory_order_relaxed) || _workersRunning.load(std::memory_order_relaxed)) {
        sendToLoggerError("Cannot change worker thread count while server is running");
        return;
    }
    if (count == 0) { sendToLoggerError("Worker thread count must be at least 1, setting to 1"); _workerThreadCount = 1; _configFlags.workerThreadsSet = true; return; }
    _workerThreadCount = count;
    _configFlags.workerThreadsSet = true;
    sendToLogger("Worker thread count set to: " + std::to_string(_workerThreadCount));
}

void Geruest::setMaxQueueSize(size_t size) {
    if (running.load(std::memory_order_relaxed) || _workersRunning.load(std::memory_order_relaxed)) {
        sendToLoggerError("Cannot change queue size while server is running");
        return;
    }
    if (size == 0) { sendToLoggerError("Queue size must be at least 1, setting to 1"); _maxQueueSize = 1; _configFlags.maxQueueSizeSet = true; return; }
    _maxQueueSize = size;
    _configFlags.maxQueueSizeSet = true;
    sendToLogger("Maximum concurrent sessions set to: " + std::to_string(_maxQueueSize));
}

void Geruest::setMaxRequestsPerConnection(size_t count) {
    if (running.load(std::memory_order_relaxed) || _workersRunning.load(std::memory_order_relaxed)) {
        sendToLoggerError("Cannot change max requests per connection while server is running");
        return;
    }
    serverData.setMaxRequestsPerConnection(count);
    _configFlags.maxRequestsPerConnectionSet = true;
    if (count == 0) {
        sendToLogger("Max requests per connection set to unlimited");
    } else {
        sendToLogger("Max requests per connection set to: " + std::to_string(count));
    }
}

void Geruest::setTextResponseCacheMaxEntryBytes(size_t bytes) {
    if (running.load(std::memory_order_relaxed) || _workersRunning.load(std::memory_order_relaxed)) {
        sendToLoggerError("Cannot change text response cache limits while server is running");
        return;
    }
    serverData.setTextResponseCacheMaxEntryBytes(bytes);
    _configFlags.textResponseCacheMaxEntryBytesSet = true;
    sendToLogger("TEXT_RESPONSE_CACHE_MAX_ENTRY_BYTES set to: " + std::to_string(bytes));
}

void Geruest::setTextResponseCacheMaxTotalBytes(size_t bytes) {
    if (running.load(std::memory_order_relaxed) || _workersRunning.load(std::memory_order_relaxed)) {
        sendToLoggerError("Cannot change text response cache limits while server is running");
        return;
    }
    serverData.setTextResponseCacheMaxTotalBytes(bytes);
    _configFlags.textResponseCacheMaxTotalBytesSet = true;
    sendToLogger("TEXT_RESPONSE_CACHE_MAX_TOTAL_BYTES set to: " + std::to_string(bytes));
}

// ========== Basic Authentication ==========

void Geruest::setBasicAuth(bool enabled) {
    serverData.getBasicAuth().setEnabled(enabled);
    sendToLogger(std::string("Basic Authentication ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::enableBasicAuth() { setBasicAuth(true); }

void Geruest::addBasicAuthUser(const std::string& username, const std::string& password) {
    serverData.getBasicAuth().addUser(username, password);
    sendToLogger("Added Basic Auth user: " + username + " (password hashed with SHA-256)");
}

void Geruest::addBasicAuthUserHashed(const std::string& username, const std::string& hashedPassword) {
    serverData.getBasicAuth().addUserHashed(username, hashedPassword);
    sendToLogger("Added Basic Auth user: " + username + " (using pre-hashed password)");
}

std::string Geruest::hashPassword(const std::string& password) { return BasicAuth::hashPassword(password); }

bool Geruest::removeBasicAuthUser(const std::string& username) {
    bool removed = serverData.getBasicAuth().removeUser(username);
    if (removed) sendToLogger("Removed Basic Auth user: " + username);
    return removed;
}

void Geruest::addProtectedPage(const std::string& path) {
    serverData.getBasicAuth().addProtectedPage(path);
    sendToLogger("Added protected page: " + path);
}

bool Geruest::removeProtectedPage(const std::string& path) {
    bool removed = serverData.getBasicAuth().removeProtectedPage(path);
    if (removed) sendToLogger("Removed protected page: " + path);
    return removed;
}

void Geruest::clearBasicAuthUsers()  { 
    serverData.getBasicAuth().clearUsers();          
    sendToLogger("Cleared all Basic Auth users"); 
}

void Geruest::clearProtectedPages()  {
    serverData.getBasicAuth().clearProtectedPages();
    sendToLogger("Cleared all protected pages");
}

// ========== Page Gates ==========

void Geruest::addGatedPage(const std::string& path, PageGateHandler gate, const std::string& redirectTo) {
    if (serverData.addPageGate(path, std::move(gate), redirectTo)) {
        sendToLogger("Added gated page: " + path);
    } else {
        sendToLoggerError("Failed to add gated page (path/handler invalid): " + path);
    }
}

void Geruest::addGatedPage(const std::string& path, AsyncPageGateHandler gate, const std::string& redirectTo) {
    if (serverData.addAsyncPageGate(path, std::move(gate), redirectTo)) {
        sendToLogger("Added gated page (async gate): " + path);
    } else {
        sendToLoggerError("Failed to add gated page (async gate invalid): " + path);
    }
}

bool Geruest::removeGatedPage(const std::string& path) {
    bool removed = serverData.removePageGate(path);
    if (removed) sendToLogger("Removed gated page: " + path);
    return removed;
}

void Geruest::clearGatedPages() {
    serverData.clearPageGates();
    sendToLogger("Cleared all gated pages");
}

// ========== Route Gates ==========

bool Geruest::removeGatedRoute(const std::string& path) {
    bool removed = serverData.removeRouteGate(path);
    if (removed) sendToLogger("Removed gated route: " + path);
    return removed;
}

void Geruest::clearGatedRoutes() {
    serverData.clearRouteGates();
    sendToLogger("Cleared all gated routes");
}

void Geruest::enableCors(const CorsOptions& options) {
    serverData.setCorsConfig(CorsConfig::fromOptions(options));
    if (serverData.getCorsConfig().isEnabled() && serverData.shouldLog(LogLevel::Info)) {
        sendToLogger("CORS enabled for " + std::to_string(options.paths.size()) + " path pattern(s)");
    }
}

}  // namespace geruest
