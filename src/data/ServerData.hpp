/**
 * @file ServerData.hpp
 * @date 11.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Composing facade for server configuration, routing, gates, and metrics.
 */

#ifndef GERUEST_SERVERDATA_HPP
#define GERUEST_SERVERDATA_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../auth/BasicAuth.hpp"
#include "GateRegistry.hpp"
#include "LanguageConfig.hpp"
#include "ObfuscationSettings.hpp"
#include "RouteRegistry.hpp"
#include "ServerMetrics.hpp"
#include "ServerTypes.hpp"
#include "database/DatabaseClient.hpp"

namespace geruest {

/**
 * Log level enumeration for filtering log output
 * None: No logging
 * Error: Only errors
 * Warning: Errors and warnings
 * Info: Errors, warnings, and informational messages
 * Debug: All messages including debug information
 */
enum class LogLevel {
    None = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4
};

class ServerData {
   public:
    using WindowMetrics = ServerMetrics::WindowMetrics;
    using LatencyStats = ServerMetrics::LatencyStats;

    ServerData();
    ServerData(const ServerData& other);
    ServerData& operator=(const ServerData& other);
    ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string root);

    std::unordered_map<std::string, RouteHandler> getRoutes();
    const std::unordered_map<std::string, RouteHandler>& getRoutes() const;

    void addRoute(const std::string& path, RouteHandler routeHandler) { _routes.addRoute(path, std::move(routeHandler)); }
    void addRouteAsync(const std::string& path, AsyncRouteHandler routeHandler) {
        _routes.addRouteAsync(path, std::move(routeHandler));
    }
    void addWebSocketRoute(const std::string& path, WebSocketHandler routeHandler) {
        _routes.addWebSocketRoute(path, std::move(routeHandler));
    }

    bool addRedirect(const std::string& from, const std::string& to, int status = 301) {
        return _routes.addRedirect(from, to, status);
    }
    size_t addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status = 301) {
        return _routes.addRedirects(redirects, status);
    }
    std::optional<std::pair<std::string, int>> findMatchingRedirect(const std::string& path) const {
        return _routes.findMatchingRedirect(path);
    }

    bool addPageGate(const std::string& path, PageGateHandler handler, const std::string& redirectTo = "") {
        return _gates.addPageGate(path, std::move(handler), redirectTo);
    }
    bool addAsyncPageGate(const std::string& path, AsyncPageGateHandler handler, const std::string& redirectTo = "") {
        return _gates.addAsyncPageGate(path, std::move(handler), redirectTo);
    }
    bool removePageGate(const std::string& path) { return _gates.removePageGate(path); }
    void clearPageGates() { _gates.clearPageGates(); }
    std::optional<PageGateRule> findMatchingPageGate(const std::string& path) const {
        return _gates.findMatchingPageGate(path);
    }
    std::optional<AsyncPageGateRule> findMatchingAsyncPageGate(const std::string& path) const {
        return _gates.findMatchingAsyncPageGate(path);
    }
    std::optional<ResolvedPageGate> findResolvedPageGate(const std::string& path) const {
        return _gates.findResolvedPageGate(path);
    }
    std::string resolvePageGateRedirect(const std::string& redirectTo, const std::string& requestPath) const {
        return _gates.resolvePageGateRedirect(redirectTo, requestPath);
    }
    bool pageRequiresAccessControl(const std::string& pagePath) const;
    std::optional<std::string> findMergedAssetOwnerPagePath(const std::string& assetRequestPath) const;

    bool addRouteGate(const std::string& path, RouteGateHandler handler) {
        return _gates.addRouteGate(path, std::move(handler));
    }
    bool addAsyncRouteGate(const std::string& path, AsyncRouteGateHandler handler) {
        return _gates.addAsyncRouteGate(path, std::move(handler));
    }
    bool removeRouteGate(const std::string& path) { return _gates.removeRouteGate(path); }
    void clearRouteGates() { _gates.clearRouteGates(); }
    std::optional<RouteGateRule> findMatchingRouteGate(const std::string& path) const {
        return _gates.findMatchingRouteGate(path);
    }
    std::optional<ResolvedRouteGate> findResolvedRouteGate(const std::string& path) const {
        return _gates.findResolvedRouteGate(path);
    }

    std::optional<RouteHandler> findMatchingRoute(const std::string& path) const {
        return _routes.findMatchingRoute(path);
    }
    std::optional<AsyncRouteHandler> findMatchingAsyncRoute(const std::string& path) const {
        return _routes.findMatchingAsyncRoute(path);
    }
    std::optional<WebSocketHandler> findMatchingWebSocketRoute(const std::string& path) const {
        return _routes.findMatchingWebSocketRoute(path);
    }

    void setWebSocketMaxMessageBytes(size_t bytes) { _routes.setWebSocketMaxMessageBytes(bytes); }
    void setWebSocketMaxFrameBytes(size_t bytes) { _routes.setWebSocketMaxFrameBytes(bytes); }
    void setWebSocketIdleTimeout(std::chrono::seconds seconds) { _routes.setWebSocketIdleTimeout(seconds); }
    void setWebSocketPingInterval(std::chrono::seconds seconds) { _routes.setWebSocketPingInterval(seconds); }
    void addWebSocketSubprotocol(std::string name) { _routes.addWebSocketSubprotocol(std::move(name)); }
    const WebSocketLimits& getWebSocketLimits() const { return _routes.getWebSocketLimits(); }
    const std::vector<std::string>& getWebSocketSubprotocols() const { return _routes.getWebSocketSubprotocols(); }

    const std::string& getRoot() const { return _root; }
    void setRoot(const std::string& newRoot) { _root = newRoot; }

    bool getRemoveComments() const { return _removeComments; }
    void setRemoveComments(bool value) { _removeComments = value; }
    void keepComments() { _removeComments = false; }

    void setMergeAssets(bool value) { _mergeAssets = value; }
    bool getMergeAssets() const { return _mergeAssets; }

    void setWebPConversion(bool value) { _webpConversion = value; }
    bool getWebPConversion() const { return _webpConversion; }
    void enableWebPConversion() { _webpConversion = true; }

    void setWebPQuality(float quality);
    float getWebPQuality() const { return _webpQuality; }

    void setMaxRequestsPerConnection(size_t value) { _maxRequestsPerConnection = value; }
    size_t getMaxRequestsPerConnection() const { return _maxRequestsPerConnection; }

    void setTextResponseCacheMaxEntryBytes(size_t bytes) { _textResponseCacheMaxEntryBytes = bytes; }
    size_t getTextResponseCacheMaxEntryBytes() const { return _textResponseCacheMaxEntryBytes; }
    void setTextResponseCacheMaxTotalBytes(size_t bytes) { _textResponseCacheMaxTotalBytes = bytes; }
    size_t getTextResponseCacheMaxTotalBytes() const { return _textResponseCacheMaxTotalBytes; }

    void enableDevMode();
    bool isDevMode() const { return _devMode; }

    void setAvailableLanguages(const std::vector<std::string>& languages) {
        _languages.setAvailableLanguages(languages);
    }
    const std::vector<std::string>& getAvailableLanguages() const { return _languages.getAvailableLanguages(); }
    const std::string& getDefaultLanguage() const { return _languages.getDefaultLanguage(); }
    bool isLanguageAvailable(const std::string& lang) const { return _languages.isLanguageAvailable(lang); }
    bool hasLanguages() const { return _languages.hasLanguages(); }
    std::optional<std::string> languagePrefixFromPath(const std::string& path) const {
        return _languages.languagePrefixFromPath(path);
    }
    std::string resolvePreferredLanguage(std::string_view acceptLanguage) const {
        return _languages.resolvePreferredLanguage(acceptLanguage);
    }
    std::string localizePathWithRequestLanguage(const std::string& path, const std::string& requestPath) const {
        return _languages.localizePathWithRequestLanguage(path, requestPath);
    }

    void setNotFoundPage(const std::string& path) { _notFoundPage = path; }
    const std::string& getNotFoundPage() const { return _notFoundPage; }
    bool hasNotFoundPage() const { return !_notFoundPage.empty(); }

    BasicAuth& getBasicAuth() { return _basicAuth; }
    const BasicAuth& getBasicAuth() const { return _basicAuth; }

    void setLogLevel(LogLevel level) { _logLevel.store(level, std::memory_order_relaxed); }
    LogLevel getLogLevel() const { return _logLevel.load(std::memory_order_relaxed); }
    bool shouldLog(LogLevel level) const {
        return static_cast<int>(level) <= static_cast<int>(_logLevel.load(std::memory_order_relaxed));
    }

    void setDatabaseClient(std::shared_ptr<db::DatabaseClient> client) { _databaseClient = std::move(client); }
    std::shared_ptr<db::DatabaseClient> getDatabaseClient() const { return _databaseClient; }

    void setObfuscationLevel(unsigned int level) { _obfuscation.setLevel(level); }
    unsigned int getObfuscationLevel() const { return _obfuscation.getLevel(); }
    void setObfuscationCacheExpiry(int days) { _obfuscation.setCacheExpiryDays(days); }
    int getObfuscationCacheExpiry() const { return _obfuscation.getCacheExpiryDays(); }
    void addObfuscationExclusion(const std::string& filename) { _obfuscation.addExclusion(filename); }
    bool isObfuscationExcluded(const std::string& filename) const { return _obfuscation.isExcluded(filename); }
    const std::vector<std::string>& getObfuscationExclusions() const { return _obfuscation.getExclusions(); }
    void addObfuscationPreserveIdent(const std::string& name) { _obfuscation.addPreserveIdent(name); }
    void addObfuscationExternGlobal(const std::string& name) { _obfuscation.addExternGlobal(name); }
    const std::unordered_set<std::string>& getObfuscationPreserveIdents() const {
        return _obfuscation.getPreserveIdents();
    }
    const std::unordered_set<std::string>& getObfuscationExternGlobals() const {
        return _obfuscation.getExternGlobals();
    }
    void loadObfuscationExternsFromText(const std::string& text) { _obfuscation.loadExternsFromText(text); }
    void setObfuscationStrictUndefined(bool v) { _obfuscation.setStrictUndefined(v); }
    bool getObfuscationStrictUndefined() const { return _obfuscation.getStrictUndefined(); }
    void setObfuscationEmitGlobalThisAssignments(bool v) { _obfuscation.setEmitGlobalThisBracket(v); }
    bool getObfuscationEmitGlobalThisAssignments() const { return _obfuscation.getEmitGlobalThisBracket(); }
    void setObfuscationValidateWithAcorn(bool v) { _obfuscation.setValidateWithAcorn(v); }
    bool getObfuscationValidateWithAcorn() const { return _obfuscation.getValidateWithAcorn(); }
    void setObfuscationAutoBracketKeys(bool v) { _obfuscation.setAutoBracketKeys(v); }
    bool getObfuscationAutoBracketKeys() const { return _obfuscation.getAutoBracketKeys(); }
    bool shouldObfuscate() const { return !_devMode && _obfuscation.getLevel() > 0; }

    static bool isMetricsExcludedPath(const std::string& path) {
        return ServerMetrics::isMetricsExcludedPath(path);
    }

    void recordRequest() const { _metrics.recordRequest(); }
    void recordError() const { _metrics.recordError(); }
    void record4xx() const { _metrics.record4xx(); }
    void record5xx() const { _metrics.record5xx(); }
    void recordQueueRejection() const { _metrics.recordQueueRejection(); }
    void recordAcceptError() const { _metrics.recordAcceptError(); }
    void recordAcceptEmfile() const { _metrics.recordAcceptEmfile(); }
    void recordFileOpenFailure() const { _metrics.recordFileOpenFailure(); }
    void recordOverloadHttpResponse() const { _metrics.recordOverloadHttpResponse(); }
    void recordQueueFill(float fillPct) const { _metrics.recordQueueFill(fillPct); }
    void incrementActiveHandlers() const { _metrics.incrementActiveHandlers(); }
    void decrementActiveHandlers() const { _metrics.decrementActiveHandlers(); }
    void recordLatency(uint32_t us) const { _metrics.recordLatency(us); }

    uint64_t getTotalRequests() const { return _metrics.getTotalRequests(); }
    uint64_t getTotalErrors() const { return _metrics.getTotalErrors(); }
    uint64_t getTotal4xx() const { return _metrics.getTotal4xx(); }
    uint64_t getTotal5xx() const { return _metrics.getTotal5xx(); }
    uint64_t getTotalInternalErrors() const { return _metrics.getTotalInternalErrors(); }
    uint64_t getQueueRejections() const { return _metrics.getQueueRejections(); }
    uint64_t getAcceptErrorsTotal() const { return _metrics.getAcceptErrorsTotal(); }
    uint64_t getAcceptEmfileTotal() const { return _metrics.getAcceptEmfileTotal(); }
    uint64_t getFileOpenFailures() const { return _metrics.getFileOpenFailures(); }
    uint64_t getOverloadHttpResponses() const { return _metrics.getOverloadHttpResponses(); }
    int64_t getActiveHandlers() const { return _metrics.getActiveHandlers(); }
    WindowMetrics getWindowMetricsHour() const { return _metrics.getWindowMetricsHour(); }
    WindowMetrics getRollingAveragePerHour() const { return _metrics.getRollingAveragePerHour(); }
    LatencyStats getLatencyStats(uint32_t windowSeconds) const { return _metrics.getLatencyStats(windowSeconds); }
    uint64_t getUptimeSeconds() const { return _metrics.getUptimeSeconds(); }
    bool loadPersistentMetricsFromFile(const std::string& path) { return _metrics.loadPersistentMetricsFromFile(path); }
    bool savePersistentMetricsToFile(const std::string& path) const { return _metrics.savePersistentMetricsToFile(path); }
    double getUptimeHoursTotal() const { return _metrics.getUptimeHoursTotal(); }

   private:
    void wireLanguagePointers_();

    LanguageConfig _languages;
    RouteRegistry _routes;
    GateRegistry _gates;
    ObfuscationSettings _obfuscation;
    ServerMetrics _metrics;

    std::string _root;
    bool _removeComments = true;
    bool _mergeAssets = false;
    bool _devMode = false;
    bool _webpConversion = false;
    float _webpQuality = 75.0f;
    size_t _maxRequestsPerConnection = 1000;
    size_t _textResponseCacheMaxEntryBytes = 512 * 1024;
    size_t _textResponseCacheMaxTotalBytes = 32 * 1024 * 1024;
    std::string _notFoundPage;
    BasicAuth _basicAuth;
    std::atomic<LogLevel> _logLevel{LogLevel::Error};
    std::shared_ptr<db::DatabaseClient> _databaseClient;
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP
