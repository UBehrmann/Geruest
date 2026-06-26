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
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../auth/BasicAuth.hpp"
#include "CorsConfig.hpp"
#include "DevAssetCache.hpp"
#include "TextResponseCache.hpp"
#include "GateRegistry.hpp"
#include "LanguageConfig.hpp"
#include "obfuscation/ObfuscationSettings.hpp"
#include "RouteRegistry.hpp"
#include "ServerMetrics.hpp"
#include "ServerTypes.hpp"

namespace geruest {
namespace db {
class DatabaseClient;
}
}

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

/** Optional redirect for log output (level, message, context e.g. client IP or "Geruest"). */
using LogSink = std::function<void(LogLevel level, std::string_view message, std::string_view context)>;

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

    void addRoute(const std::string& path, RouteHandler routeHandler);
    void addRoute(const std::string& path, AsyncRouteHandler routeHandler);
    void addWebSocketRoute(const std::string& path, WebSocketHandler routeHandler);

    bool addRedirect(const std::string& from, const std::string& to, int status = 301);
    size_t addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status = 301);
    std::optional<std::pair<std::string, int>> findMatchingRedirect(const std::string& path) const;

    bool addPageGate(const std::string& path, PageGateHandler handler, const std::string& redirectTo = "");
    bool addAsyncPageGate(const std::string& path, AsyncPageGateHandler handler, const std::string& redirectTo = "");
    bool removePageGate(const std::string& path);
    void clearPageGates();
    std::optional<PageGateRule> findMatchingPageGate(const std::string& path) const;
    std::optional<AsyncPageGateRule> findMatchingAsyncPageGate(const std::string& path) const;
    std::optional<ResolvedPageGate> findResolvedPageGate(const std::string& path) const;
    std::string resolvePageGateRedirect(const std::string& redirectTo, const std::string& requestPath) const;
    bool pageRequiresAccessControl(const std::string& pagePath) const;
    std::optional<std::string> findMergedAssetOwnerPagePath(const std::string& assetRequestPath) const;

    bool addRouteGate(const std::string& path, RouteGateHandler handler);
    bool addAsyncRouteGate(const std::string& path, AsyncRouteGateHandler handler);
    bool removeRouteGate(const std::string& path);
    void clearRouteGates();
    std::optional<RouteGateRule> findMatchingRouteGate(const std::string& path) const;
    std::optional<ResolvedRouteGate> findResolvedRouteGate(const std::string& path) const;

    std::optional<RouteHandler> findMatchingRoute(const std::string& path) const;
    std::optional<AsyncRouteHandler> findMatchingAsyncRoute(const std::string& path) const;
    std::optional<WebSocketHandler> findMatchingWebSocketRoute(const std::string& path) const;

    void setWebSocketMaxMessageBytes(size_t bytes);
    void setWebSocketMaxFrameBytes(size_t bytes);
    void setWebSocketIdleTimeout(std::chrono::seconds seconds);
    void setWebSocketPingInterval(std::chrono::seconds seconds);
    void addWebSocketSubprotocol(std::string name);
    const WebSocketLimits& getWebSocketLimits() const;
    const std::vector<std::string>& getWebSocketSubprotocols() const;

    const std::string& getRoot() const { return _root; }
    void setRoot(const std::string& newRoot);

    bool getRemoveComments() const { return _removeComments; }
    void setRemoveComments(bool value) { _removeComments = value; }
    void keepComments() { _removeComments = false; }

    void setMergeAssets(bool value);
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

    void setStaticCacheMaxAge(int seconds);
    int getStaticCacheMaxAge() const { return _staticCacheMaxAge; }

    void setStaticHtmlCacheMaxAge(int seconds);
    int getStaticHtmlCacheMaxAge() const { return _staticHtmlCacheMaxAge; }

    void setCacheBustToken(const std::string& token);
    const std::string& getCacheBustToken() const { return _cacheBustToken; }
    void ensureCacheBustToken();

    void enableDevMode();
    bool isDevMode() const { return _devMode; }

    DevAssetCache& devAssetCache() { return _devAssetCache; }
    const DevAssetCache& devAssetCache() const { return _devAssetCache; }

    TextResponseCache& textResponseCache() { return _textResponseCache; }
    const TextResponseCache& textResponseCache() const { return _textResponseCache; }

    void setAvailableLanguages(const std::vector<std::string>& languages);
    const std::vector<std::string>& getAvailableLanguages() const;
    const std::string& getDefaultLanguage() const;
    bool isLanguageAvailable(const std::string& lang) const;
    bool hasLanguages() const;
    std::optional<std::string> languagePrefixFromPath(const std::string& path) const;
    std::string resolvePreferredLanguage(std::string_view acceptLanguage) const;
    std::string localizePathWithRequestLanguage(const std::string& path, const std::string& requestPath) const;

    void setNotFoundPage(const std::string& path) { _notFoundPage = path; }
    const std::string& getNotFoundPage() const { return _notFoundPage; }
    bool hasNotFoundPage() const { return !_notFoundPage.empty(); }

    void setCorsConfig(CorsConfig config) { _corsConfig = std::move(config); }
    const CorsConfig& getCorsConfig() const { return _corsConfig; }

    BasicAuth& getBasicAuth() { return _basicAuth; }
    const BasicAuth& getBasicAuth() const { return _basicAuth; }

    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    bool shouldLog(LogLevel level) const;

    void setLogSink(LogSink sink);
    void clearLogSink();
    void emitLog(LogLevel level, std::string_view message, std::string_view context = {}) const;

    void setDatabaseClient(std::shared_ptr<db::DatabaseClient> client);
    std::shared_ptr<db::DatabaseClient> getDatabaseClient() const;

    void setObfuscationLevel(unsigned int level);
    unsigned int getObfuscationLevel() const;
    void setObfuscationCacheExpiry(int days);
    int getObfuscationCacheExpiry() const;
    void addObfuscationExclusion(const std::string& filename);
    bool isObfuscationExcluded(const std::string& filename) const;
    const std::vector<std::string>& getObfuscationExclusions() const;
    void addObfuscationPreserveIdent(const std::string& name);
    void addObfuscationExternGlobal(const std::string& name);
    const std::unordered_set<std::string>& getObfuscationPreserveIdents() const;
    const std::unordered_set<std::string>& getObfuscationExternGlobals() const;
    void loadObfuscationExternsFromText(const std::string& text);
    void setObfuscationStrictUndefined(bool v);
    bool getObfuscationStrictUndefined() const;
    void setObfuscationEmitGlobalThisAssignments(bool v);
    bool getObfuscationEmitGlobalThisAssignments() const;
    void setObfuscationValidateWithAcorn(bool v);
    bool getObfuscationValidateWithAcorn() const;
    void setObfuscationAutoBracketKeys(bool v);
    bool getObfuscationAutoBracketKeys() const;
    bool shouldObfuscate() const;

    static bool isMetricsExcludedPath(const std::string& path);

    void recordRequest() const;
    void recordError() const;
    void record4xx() const;
    void record5xx() const;
    void recordQueueRejection() const;
    void recordAcceptError() const;
    void recordAcceptEmfile() const;
    void recordFileOpenFailure() const;
    void recordOverloadHttpResponse() const;
    void recordQueueFill(float fillPct) const;
    void incrementActiveHandlers() const;
    void decrementActiveHandlers() const;
    void recordLatency(uint32_t us) const;

    uint64_t getTotalRequests() const;
    uint64_t getTotalErrors() const;
    uint64_t getTotal4xx() const;
    uint64_t getTotal5xx() const;
    uint64_t getTotalInternalErrors() const;
    uint64_t getQueueRejections() const;
    uint64_t getAcceptErrorsTotal() const;
    uint64_t getAcceptEmfileTotal() const;
    uint64_t getFileOpenFailures() const;
    uint64_t getOverloadHttpResponses() const;
    int64_t getActiveHandlers() const;
    WindowMetrics getWindowMetricsHour() const;
    WindowMetrics getRollingAveragePerHour() const;
    LatencyStats getLatencyStats(uint32_t windowSeconds) const;
    uint64_t getUptimeSeconds() const;
    bool loadPersistentMetricsFromFile(const std::string& path);
    bool savePersistentMetricsToFile(const std::string& path) const;
    double getUptimeHoursTotal() const;

   private:
    void wireLanguagePointers_();
    void clearMergedAssetOwnerCache_();
    bool mightNeedMergedAssetOwnerLookup() const;

    LanguageConfig _languages;
    RouteRegistry _routes;
    GateRegistry _gates;
    ObfuscationSettings _obfuscation;
    ServerMetrics _metrics;
    DevAssetCache _devAssetCache;
    TextResponseCache _textResponseCache;

    std::string _root;
    bool _removeComments = true;
    bool _mergeAssets = false;
    bool _devMode = false;
    bool _webpConversion = false;
    float _webpQuality = 75.0f;
    size_t _maxRequestsPerConnection = 1000;
    size_t _textResponseCacheMaxEntryBytes = 512 * 1024;
    size_t _textResponseCacheMaxTotalBytes = 32 * 1024 * 1024;
    int _staticCacheMaxAge = 31536000;
    int _staticHtmlCacheMaxAge = 0;
    std::string _cacheBustToken;
    bool _cacheBustTokenManual = false;
    std::string _notFoundPage;
    CorsConfig _corsConfig;
    BasicAuth _basicAuth;
    std::atomic<LogLevel> _logLevel{LogLevel::Error};
    LogSink _logSink;
    std::shared_ptr<db::DatabaseClient> _databaseClient;

    mutable std::unordered_map<std::string, std::optional<std::string>> _mergedAssetOwnerCache;
    mutable std::mutex _mergedAssetOwnerCacheMutex;
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP
