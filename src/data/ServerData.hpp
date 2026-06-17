/**
 * @file ServerData.hpp
 * @date 11.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief This file contains the ServerData struct, which holds the server's data
 */

#ifndef GERUEST_SERVERDATA_HPP
#define GERUEST_SERVERDATA_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/asio/awaitable.hpp>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "database/DatabaseClient.hpp"
#include "../auth/BasicAuth.hpp"
#include "parser/JSONParser.hpp"
#include "server/WebSocketTypes.hpp"

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

class WebSocketConnection;

using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;
using AsyncResponse = boost::asio::awaitable<HTTPResponse>;
using AsyncRouteHandler = std::function<AsyncResponse(const HTTPRequest&)>;
using WebSocketHandler =
    std::function<boost::asio::awaitable<void>(WebSocketConnection&, const HTTPRequest&)>;

/** Returns true when access should be granted. */
using PageGateHandler = std::function<bool(const HTTPRequest&)>;
using AsyncPageGateAccess = boost::asio::awaitable<bool>;
using AsyncPageGateHandler = std::function<AsyncPageGateAccess(const HTTPRequest&)>;
using RouteGateHandler = PageGateHandler;

struct PageGateRule {
    PageGateHandler handler;
    std::string redirectTo;
};

struct AsyncPageGateRule {
    AsyncPageGateHandler handler;
    std::string redirectTo;
};

/** Winning page gate after sync/async and wildcard resolution. */
struct ResolvedPageGate {
    bool async = false;
    PageGateHandler syncHandler;
    AsyncPageGateHandler asyncHandler;
    std::string redirectTo;
};

struct RouteGateRule {
    RouteGateHandler handler;
};

/** Default redirect when a page gate denies access and no custom target is set. */
inline std::string defaultPageGateRedirect(const std::string& requestPath) {
    if (requestPath.size() >= 4 && requestPath[0] == '/' &&
        std::isalpha(static_cast<unsigned char>(requestPath[1])) &&
        std::isalpha(static_cast<unsigned char>(requestPath[2])) && requestPath[3] == '/') {
        return requestPath.substr(0, 4);
    }
    return "/";
}

// class with the server data
class ServerData {
   private:
    struct RedirectRule {
        std::string target;
        int status = 301;
    };

    std::unordered_map<std::string, RouteHandler> _routes;
    std::unordered_map<std::string, RouteHandler> _wildcardRoutes;
    std::unordered_map<std::string, AsyncRouteHandler> _asyncRoutes;
    std::unordered_map<std::string, AsyncRouteHandler> _asyncWildcardRoutes;
    std::unordered_map<std::string, WebSocketHandler> _webSocketRoutes;
    std::unordered_map<std::string, WebSocketHandler> _webSocketWildcardRoutes;
    WebSocketLimits _webSocketLimits{};
    std::vector<std::string> _webSocketSubprotocols;
    std::unordered_map<std::string, RedirectRule> _redirects;
    std::unordered_map<std::string, RedirectRule> _wildcardRedirects;
    std::unordered_map<std::string, PageGateRule> _pageGates;
    std::unordered_map<std::string, PageGateRule> _wildcardPageGates;
    std::unordered_map<std::string, AsyncPageGateRule> _asyncPageGates;
    std::unordered_map<std::string, AsyncPageGateRule> _wildcardAsyncPageGates;
    std::unordered_map<std::string, RouteGateRule> _routeGates;
    std::unordered_map<std::string, RouteGateRule> _wildcardRouteGates;
    std::string _root;
    bool _removeComments = true;    // Remove comments from built files
    bool _mergeAssets = false;      // Automatic CSS/JS merging per page
    bool _devMode = false;          // Development mode (no file caching, verbose logging)
    bool _webpConversion = false;   // Automatic PNG/JPG to WebP conversion
    float _webpQuality = 75.0f;     // WebP encoding quality (0-100, default 75%)
    size_t _maxRequestsPerConnection = 1000;  // Keep-alive request cap (0 = unlimited)
    /** Serialized text response (html/js/css) cache: max one entry size (0 = do not cache). */
    size_t _textResponseCacheMaxEntryBytes = 512 * 1024;
    /** Serialized text response cache: max sum of payload bytes across entries (0 = do not cache). */
    size_t _textResponseCacheMaxTotalBytes = 32 * 1024 * 1024;
    unsigned int _obfuscationLevel = 0;  // JS obfuscation level (0=disabled, 1-3=increasing complexity)
    int _obfuscationCacheExpiryDays = 7;  // Days to keep obfuscated files cached
    std::vector<std::string> _obfuscationExclusions;  // Files excluded from obfuscation and merging
    std::unordered_set<std::string> _obfuscationPreserveIdents;   // Never rename (API / cross-chunk)
    std::unordered_set<std::string> _obfuscationExternGlobals;    // Assumed global at runtime
    bool _obfuscationStrictUndefined = false;   // Throw if obfuscator reports undefined free identifiers
    bool _obfuscationEmitGlobalThisBracket = false;  // Append globalThis['name']=name for preserved top-level
    bool _obfuscationValidateWithAcorn = false; // Optional parse via node+acorn after transform
    bool _obfuscationAutoBracketKeys = true;    // Add ['name'] / ["name"] keys to preserve set
    std::vector<std::string> _availableLanguages;
    std::string _defaultLanguage;
    std::string _notFoundPage;
    BasicAuth _basicAuth;
    std::atomic<LogLevel> _logLevel{LogLevel::Error};  // Thread-safe log level (can be changed at runtime)
    std::shared_ptr<db::DatabaseClient> _databaseClient;

    // ========== Metrics (mutable: incremented via const ServerData& in Handler) ==========
    mutable std::atomic<uint64_t> _totalRequests{0};
    mutable std::atomic<uint64_t> _total4xx{0};
    mutable std::atomic<uint64_t> _total5xx{0};
    mutable std::atomic<uint64_t> _totalInternalErrors{0};
    mutable std::atomic<uint64_t> _queueRejections{0};
    mutable std::atomic<uint64_t> _acceptErrorsTotal{0};
    mutable std::atomic<uint64_t> _acceptEmfileTotal{0};
    mutable std::atomic<uint64_t> _fileOpenFailures{0};
    mutable std::atomic<uint64_t> _overloadHttpResponses{0};
    mutable std::atomic<int64_t>  _activeHandlers{0};
    std::chrono::steady_clock::time_point _startTime{std::chrono::steady_clock::now()};

    // 60 rolling minute buckets (last hour)
    struct RollingBucket {
        uint32_t epoch      = 0;
        uint32_t requests   = 0;
        uint32_t errors_4xx = 0;
        uint32_t errors_5xx = 0;
        uint32_t errors_int = 0;
        float    fill_sum   = 0.f;  // sum of queue fill% samples
        uint32_t fill_count = 0;
    };
    mutable std::mutex _metricsMutex;
    mutable std::array<RollingBucket, 60> _minBuckets{};

    // Timestamped latency ring buffer (values in microseconds)
    struct LatencySample { uint32_t epoch_s = 0; uint32_t us = 0; };
    static constexpr size_t _LAT_CAP = 10000;
    mutable std::array<LatencySample, _LAT_CAP> _latSamples{};
    mutable size_t _latHead{0};
    mutable size_t _latCount{0};

    /// Cumulative process uptime (seconds) through the last successful metrics save; loaded from disk at startup.
    uint64_t _lifetimeUptimeBaselineSeconds{0};

    /**
     * Check if a path matches a wildcard pattern
     * @param pattern The route pattern (may contain *)
     * @param path The actual request path
     * @return true if the path matches the pattern
     */
    bool matchesWildcardPattern(const std::string& pattern, const std::string& path) const {
        // If no wildcards, this should have been caught by exact match
        if (pattern.find('*') == std::string::npos) {
            return pattern == path;
        }

        return matchWildcard(pattern.c_str(), path.c_str());
    }

    /**
     * Recursive wildcard matching algorithm
     * @param pattern Pattern with potential wildcards
     * @param text Text to match against
     * @return true if text matches pattern
     */
    bool matchWildcard(const char* pattern, const char* text) const {
        // If we reach end of pattern
        if (*pattern == '\0') {
            return *text == '\0';  // Match only if we also reached end of text
        }

        // If pattern contains '*'
        if (*pattern == '*') {
            // Skip consecutive '*' characters
            while (*pattern == '*') {
                pattern++;
            }

            // If pattern ends with '*', it matches everything
            if (*pattern == '\0') {
                return true;
            }

            // Try to match '*' with empty string, single character, or multiple characters
            while (*text != '\0') {
                if (matchWildcard(pattern, text)) {
                    return true;
                }
                text++;
            }

            return matchWildcard(pattern, text);
        }

        // If current characters match, continue with next characters
        if (*text != '\0' && *pattern == *text) {
            return matchWildcard(pattern + 1, text + 1);
        }

        // Characters don't match
        return false;
    }

    std::optional<std::string> extractWildcardCapture(const std::string& pattern, const std::string& path) const {
        const size_t firstStar = pattern.find('*');
        if (firstStar == std::string::npos) {
            if (pattern == path) {
                return std::string();
            }
            return std::nullopt;
        }

        if (pattern.find('*', firstStar + 1) != std::string::npos) {
            return std::nullopt;
        }

        const std::string prefix = pattern.substr(0, firstStar);
        const std::string suffix = pattern.substr(firstStar + 1);

        if (path.size() < prefix.size() + suffix.size()) {
            return std::nullopt;
        }

        if (path.compare(0, prefix.size(), prefix) != 0) {
            return std::nullopt;
        }

        if (!suffix.empty() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
            return std::nullopt;
        }

        return path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
    }

    std::string applyWildcardCapture(const std::string& target, const std::string& capture) const {
        if (target.find('*') == std::string::npos) {
            return target;
        }

        std::string resolved;
        resolved.reserve(target.size() + capture.size());
        for (char c : target) {
            if (c == '*') {
                resolved += capture;
            } else {
                resolved += c;
            }
        }
        return resolved;
    }

    static bool isLikelyExternalTarget(const std::string& target) {
        return target.rfind("http://", 0) == 0
            || target.rfind("https://", 0) == 0
            || target.rfind("//", 0) == 0;
    }

    std::optional<std::string> extractSupportedLanguagePrefix(const std::string& path) const {
        if (path.size() < 3 || path[0] != '/') {
            return std::nullopt;
        }

        const auto slashPos = path.find('/', 1);
        const size_t langLen = (slashPos == std::string::npos) ? (path.size() - 1) : (slashPos - 1);
        if (langLen != 2) {
            return std::nullopt;
        }

        const char c1 = path[1];
        const char c2 = path[2];
        if (!std::isalpha(static_cast<unsigned char>(c1)) || !std::isalpha(static_cast<unsigned char>(c2))) {
            return std::nullopt;
        }

        const std::string lang = path.substr(1, 2);
        if (!isLanguageAvailable(lang)) {
            return std::nullopt;
        }

        return lang;
    }

    bool hasSupportedLanguagePrefixInTarget(const std::string& target) const {
        return extractSupportedLanguagePrefix(target).has_value();
    }

    std::optional<std::string> stripSupportedLanguagePrefix(const std::string& path) const {
        const auto lang = extractSupportedLanguagePrefix(path);
        if (!lang.has_value()) {
            return std::nullopt;
        }

        if (path.size() == 3) {
            return std::string("/");
        }

        return path.substr(3);
    }

    std::string normalizeRedirectTargetLanguage(const std::string& target, const std::string& requestPath) const {
        if (!hasLanguages() || target.empty()) {
            return target;
        }

        if (isLikelyExternalTarget(target)) {
            return target;
        }

        if (target[0] != '/') {
            return target;
        }

        if (hasSupportedLanguagePrefixInTarget(target)) {
            return target;
        }

        const auto requestLang = extractSupportedLanguagePrefix(requestPath);
        if (!requestLang.has_value()) {
            return target;
        }

        if (target == "/") {
            return "/" + *requestLang + "/";
        }

        return "/" + *requestLang + target;
    }

    template<typename Rule>
    std::optional<Rule> findMatchingGateImpl(const std::unordered_map<std::string, Rule>& exactGates,
                                             const std::unordered_map<std::string, Rule>& wildcardGates,
                                             const std::string& path) const {
        auto exactMatch = exactGates.find(path);
        if (exactMatch != exactGates.end()) {
            return exactMatch->second;
        }

        size_t bestPatternLength = 0;
        std::optional<std::string> bestPattern;
        std::optional<Rule> bestMatch;
        for (const auto& gate : wildcardGates) {
            const std::string& pattern = gate.first;
            if (!matchesWildcardPattern(pattern, path)) {
                continue;
            }
            const bool better = !bestMatch.has_value() || pattern.size() > bestPatternLength ||
                                (pattern.size() == bestPatternLength && bestPattern.has_value() &&
                                 pattern < *bestPattern);
            if (better) {
                bestPatternLength = pattern.size();
                bestPattern = pattern;
                bestMatch = gate.second;
            }
        }
        if (bestMatch.has_value()) {
            return bestMatch;
        }

        const auto strippedPath = stripSupportedLanguagePrefix(path);
        if (!strippedPath.has_value()) {
            return std::nullopt;
        }

        exactMatch = exactGates.find(*strippedPath);
        if (exactMatch != exactGates.end()) {
            return exactMatch->second;
        }

        bestPatternLength = 0;
        bestPattern.reset();
        bestMatch.reset();
        for (const auto& gate : wildcardGates) {
            const std::string& pattern = gate.first;
            if (!matchesWildcardPattern(pattern, *strippedPath)) {
                continue;
            }
            const bool better = !bestMatch.has_value() || pattern.size() > bestPatternLength ||
                                (pattern.size() == bestPatternLength && bestPattern.has_value() &&
                                 pattern < *bestPattern);
            if (better) {
                bestPatternLength = pattern.size();
                bestPattern = pattern;
                bestMatch = gate.second;
            }
        }
        return bestMatch;
    }

    template<typename Rule>
    void storeGateRule(const std::string& path, Rule rule, std::unordered_map<std::string, Rule>& exactGates,
                       std::unordered_map<std::string, Rule>& wildcardGates) {
        if (path.find('*') != std::string::npos) {
            wildcardGates[path] = std::move(rule);
        } else {
            exactGates[path] = std::move(rule);
        }
    }

    std::optional<ResolvedPageGate> findBestWildcardPageGate(const std::string& path) const {
        size_t bestPatternLength = 0;
        std::optional<std::string> bestPattern;
        std::optional<ResolvedPageGate> bestMatch;

        auto consider = [&](const std::string& pattern, ResolvedPageGate resolved) {
            const bool better = !bestMatch.has_value() || pattern.size() > bestPatternLength ||
                                (pattern.size() == bestPatternLength && bestPattern.has_value() &&
                                 pattern < *bestPattern);
            if (better) {
                bestPatternLength = pattern.size();
                bestPattern = pattern;
                bestMatch = std::move(resolved);
            }
        };

        for (const auto& gate : _wildcardAsyncPageGates) {
            const std::string& pattern = gate.first;
            if (matchesWildcardPattern(pattern, path)) {
                consider(pattern, ResolvedPageGate{true, {}, gate.second.handler, gate.second.redirectTo});
            }
        }
        for (const auto& gate : _wildcardPageGates) {
            const std::string& pattern = gate.first;
            if (matchesWildcardPattern(pattern, path)) {
                consider(pattern, ResolvedPageGate{false, gate.second.handler, {}, gate.second.redirectTo});
            }
        }

        return bestMatch;
    }

    std::optional<ResolvedPageGate> resolveExactPageGate(const std::string& lookupPath) const {
        const auto asyncIt = _asyncPageGates.find(lookupPath);
        if (asyncIt != _asyncPageGates.end()) {
            return ResolvedPageGate{true, {}, asyncIt->second.handler, asyncIt->second.redirectTo};
        }
        const auto syncIt = _pageGates.find(lookupPath);
        if (syncIt != _pageGates.end()) {
            return ResolvedPageGate{false, syncIt->second.handler, {}, syncIt->second.redirectTo};
        }
        return std::nullopt;
    }

    std::optional<std::pair<std::string, int>> findMatchingWildcardRedirect(const std::string& path,
                                                                            const std::string& requestPath) const {
        size_t bestPatternLength = 0;
        std::optional<std::pair<std::string, int>> bestMatch;

        for (const auto& redirect : _wildcardRedirects) {
            const std::string& pattern = redirect.first;
            if (!matchesWildcardPattern(pattern, path)) {
                continue;
            }

            std::optional<std::string> capture = extractWildcardCapture(pattern, path);
            if (!capture.has_value()) {
                continue;
            }

            const std::string capturedTarget = applyWildcardCapture(redirect.second.target, *capture);
            const std::string resolvedTarget = normalizeRedirectTargetLanguage(capturedTarget, requestPath);
            if (!bestMatch.has_value() || pattern.size() > bestPatternLength) {
                bestPatternLength = pattern.size();
                bestMatch = std::make_pair(resolvedTarget, redirect.second.status);
            }
        }

        return bestMatch;
    }

    bool hasRedirectLoop(const std::string& startPath) const {
        std::set<std::string> visited;
        std::string current = startPath;

        constexpr size_t maxRedirectHops = 32;
        for (size_t hop = 0; hop < maxRedirectHops; ++hop) {
            if (!visited.insert(current).second) {
                return true;
            }

            auto next = findMatchingRedirect(current);
            if (!next.has_value()) {
                return false;
            }

            if (isLikelyExternalTarget(next->first)) {
                return false;
            }

            if (next->first.empty() || next->first[0] != '/') {
                return false;
            }

            current = next->first;
        }

        return true;
    }

    // Bucket write helper — must be called under _metricsMutex
    void _writeBuckets(uint32_t epochS, uint32_t epochM,
                       uint32_t req, uint32_t e4, uint32_t e5, uint32_t ei,
                       float qFill, uint32_t qCnt) const {
        (void)epochS; // suppress unused parameter warning
        auto apply = [](RollingBucket& b, uint32_t ep,
                        uint32_t req_, uint32_t e4_, uint32_t e5_, uint32_t ei_,
                        float qFill_, uint32_t qCnt_) {
            if (b.epoch != ep) b = RollingBucket{ep,0,0,0,0,0.f,0};
            b.requests   += req_;  b.errors_4xx += e4_;
            b.errors_5xx += e5_;   b.errors_int  += ei_;
            b.fill_sum   += qFill_; b.fill_count += qCnt_;
        };
        apply(_minBuckets[epochM % 60], epochM, req, e4, e5, ei, qFill, qCnt);
    }

    // Returns {epoch_seconds, epoch_minutes}
    static std::pair<uint32_t, uint32_t> _nowEpochs() {
        const auto now = std::chrono::system_clock::now();
        const uint32_t es = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        return {es, es / 60};
    }

   public:
    ServerData() = default;

    // Custom copy constructor needed because std::atomic is not copyable
    ServerData(const ServerData& other)
        : _routes(other._routes),
          _wildcardRoutes(other._wildcardRoutes),
          _asyncRoutes(other._asyncRoutes),
          _asyncWildcardRoutes(other._asyncWildcardRoutes),
          _webSocketRoutes(other._webSocketRoutes),
          _webSocketWildcardRoutes(other._webSocketWildcardRoutes),
          _webSocketLimits(other._webSocketLimits),
          _webSocketSubprotocols(other._webSocketSubprotocols),
          _redirects(other._redirects),
          _wildcardRedirects(other._wildcardRedirects),
          _pageGates(other._pageGates),
          _wildcardPageGates(other._wildcardPageGates),
          _asyncPageGates(other._asyncPageGates),
          _wildcardAsyncPageGates(other._wildcardAsyncPageGates),
          _routeGates(other._routeGates),
          _wildcardRouteGates(other._wildcardRouteGates),
          _root(other._root),
          _removeComments(other._removeComments),
          _mergeAssets(other._mergeAssets),
          _devMode(other._devMode),
          _webpConversion(other._webpConversion),
          _webpQuality(other._webpQuality),
          _maxRequestsPerConnection(other._maxRequestsPerConnection),
          _textResponseCacheMaxEntryBytes(other._textResponseCacheMaxEntryBytes),
          _textResponseCacheMaxTotalBytes(other._textResponseCacheMaxTotalBytes),
          _obfuscationLevel(other._obfuscationLevel),
          _obfuscationCacheExpiryDays(other._obfuscationCacheExpiryDays),
          _obfuscationExclusions(other._obfuscationExclusions),
          _obfuscationPreserveIdents(other._obfuscationPreserveIdents),
          _obfuscationExternGlobals(other._obfuscationExternGlobals),
          _obfuscationStrictUndefined(other._obfuscationStrictUndefined),
          _obfuscationEmitGlobalThisBracket(other._obfuscationEmitGlobalThisBracket),
          _obfuscationValidateWithAcorn(other._obfuscationValidateWithAcorn),
          _obfuscationAutoBracketKeys(other._obfuscationAutoBracketKeys),
          _availableLanguages(other._availableLanguages),
          _defaultLanguage(other._defaultLanguage),
          _notFoundPage(other._notFoundPage),
          _basicAuth(other._basicAuth),
          _logLevel(other._logLevel.load(std::memory_order_relaxed)),
          _databaseClient(other._databaseClient) {}

    // Custom copy assignment operator needed because std::atomic is not copyable
    ServerData& operator=(const ServerData& other) {
        if (this != &other) {
            _routes = other._routes;
            _wildcardRoutes = other._wildcardRoutes;
            _asyncRoutes = other._asyncRoutes;
            _asyncWildcardRoutes = other._asyncWildcardRoutes;
            _webSocketRoutes = other._webSocketRoutes;
            _webSocketWildcardRoutes = other._webSocketWildcardRoutes;
            _webSocketLimits = other._webSocketLimits;
            _webSocketSubprotocols = other._webSocketSubprotocols;
            _redirects = other._redirects;
            _wildcardRedirects = other._wildcardRedirects;
            _pageGates = other._pageGates;
            _wildcardPageGates = other._wildcardPageGates;
            _asyncPageGates = other._asyncPageGates;
            _wildcardAsyncPageGates = other._wildcardAsyncPageGates;
            _routeGates = other._routeGates;
            _wildcardRouteGates = other._wildcardRouteGates;
            _root = other._root;
            _removeComments = other._removeComments;
            _mergeAssets = other._mergeAssets;
            _devMode = other._devMode;
            _webpConversion = other._webpConversion;
            _webpQuality = other._webpQuality;
            _maxRequestsPerConnection = other._maxRequestsPerConnection;
            _textResponseCacheMaxEntryBytes = other._textResponseCacheMaxEntryBytes;
            _textResponseCacheMaxTotalBytes = other._textResponseCacheMaxTotalBytes;
            _obfuscationLevel = other._obfuscationLevel;
            _obfuscationCacheExpiryDays = other._obfuscationCacheExpiryDays;
            _obfuscationExclusions = other._obfuscationExclusions;
            _obfuscationPreserveIdents = other._obfuscationPreserveIdents;
            _obfuscationExternGlobals = other._obfuscationExternGlobals;
            _obfuscationStrictUndefined = other._obfuscationStrictUndefined;
            _obfuscationEmitGlobalThisBracket = other._obfuscationEmitGlobalThisBracket;
            _obfuscationValidateWithAcorn = other._obfuscationValidateWithAcorn;
            _obfuscationAutoBracketKeys = other._obfuscationAutoBracketKeys;
            _availableLanguages = other._availableLanguages;
            _defaultLanguage = other._defaultLanguage;
            _notFoundPage = other._notFoundPage;
            _basicAuth = other._basicAuth;
            _logLevel.store(other._logLevel.load(std::memory_order_relaxed), std::memory_order_relaxed);
            _databaseClient = other._databaseClient;
        }
        return *this;
    }

    ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string root)
        : _routes(routes), _root(std::move(root)) {}

    std::unordered_map<std::string, RouteHandler> getRoutes() {
        // For backward compatibility, merge both maps
        // Note: Returns a new map each time for thread safety
        // Consider deprecating this method in favor of the new findMatchingRoute
        std::unordered_map<std::string, RouteHandler> merged;
        merged.insert(_routes.begin(), _routes.end());
        merged.insert(_wildcardRoutes.begin(), _wildcardRoutes.end());
        return merged;
    }

    const std::unordered_map<std::string, RouteHandler>& getRoutes() const {
        // For backward compatibility, return exact routes only
        // This maintains existing behavior for const access
        return _routes;
    }

    void addRoute(const std::string& path, RouteHandler routeHandler) {
        // Separate wildcard routes from exact routes for performance
        if (path.find('*') != std::string::npos) {
            _wildcardRoutes[path] = std::move(routeHandler);
        } else {
            _routes[path] = std::move(routeHandler);
        }
    }

    void addRouteAsync(const std::string& path, AsyncRouteHandler routeHandler) {
        if (path.find('*') != std::string::npos) {
            _asyncWildcardRoutes[path] = std::move(routeHandler);
        } else {
            _asyncRoutes[path] = std::move(routeHandler);
        }
    }

    void addWebSocketRoute(const std::string& path, WebSocketHandler routeHandler) {
        if (path.find('*') != std::string::npos) {
            _webSocketWildcardRoutes[path] = std::move(routeHandler);
        } else {
            _webSocketRoutes[path] = std::move(routeHandler);
        }
    }

    bool addRedirect(const std::string& from, const std::string& to, int status = 301) {
        if (from.empty()) {
            return false;
        }

        if (to.empty()) {
            return false;
        }

        if (status != 301 && status != 302) {
            status = 301;
        }

        RedirectRule rule{to, status};
        if (from.find('*') != std::string::npos) {
            _wildcardRedirects[from] = std::move(rule);
        } else {
            _redirects[from] = std::move(rule);
        }

        std::string loopProbePath = from;
        const size_t wildcardPos = loopProbePath.find('*');
        if (wildcardPos != std::string::npos) {
            loopProbePath.replace(wildcardPos, 1, "loop-check");
        }

        if (hasRedirectLoop(loopProbePath)) {
            if (from.find('*') != std::string::npos) {
                _wildcardRedirects.erase(from);
            } else {
                _redirects.erase(from);
            }
            return false;
        }

        return true;
    }

    size_t addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status = 301) {
        size_t addedCount = 0;
        for (const auto& entry : redirects) {
            if (addRedirect(entry.first, entry.second, status)) {
                ++addedCount;
            }
        }
        return addedCount;
    }

    std::optional<std::pair<std::string, int>> findMatchingRedirect(const std::string& path) const {
        // Priority rule 1: exact redirect first
        auto exactMatch = _redirects.find(path);
        if (exactMatch != _redirects.end()) {
            return std::make_pair(normalizeRedirectTargetLanguage(exactMatch->second.target, path),
                                  exactMatch->second.status);
        }

        // Priority rule 2: wildcard redirect after exact redirect
        auto wildcardMatch = findMatchingWildcardRedirect(path, path);
        if (wildcardMatch.has_value()) {
            return wildcardMatch;
        }

        // If request starts with supported language prefix, retry redirects on stripped path.
        const auto strippedPath = stripSupportedLanguagePrefix(path);
        if (!strippedPath.has_value()) {
            return std::nullopt;
        }

        exactMatch = _redirects.find(*strippedPath);
        if (exactMatch != _redirects.end()) {
            return std::make_pair(normalizeRedirectTargetLanguage(exactMatch->second.target, path),
                                  exactMatch->second.status);
        }

        return findMatchingWildcardRedirect(*strippedPath, path);
    }

    bool addPageGate(const std::string& path, PageGateHandler handler, const std::string& redirectTo = "") {
        if (path.empty() || !handler) {
            return false;
        }
        storeGateRule(path, PageGateRule{std::move(handler), redirectTo}, _pageGates, _wildcardPageGates);
        return true;
    }

    bool addAsyncPageGate(const std::string& path, AsyncPageGateHandler handler,
                          const std::string& redirectTo = "") {
        if (path.empty() || !handler) {
            return false;
        }
        storeGateRule(path, AsyncPageGateRule{std::move(handler), redirectTo}, _asyncPageGates,
                      _wildcardAsyncPageGates);
        return true;
    }

    bool removePageGate(const std::string& path) {
        bool removed = false;
        if (path.find('*') != std::string::npos) {
            removed = _wildcardPageGates.erase(path) > 0;
            removed = _wildcardAsyncPageGates.erase(path) > 0 || removed;
        } else {
            removed = _pageGates.erase(path) > 0;
            removed = _asyncPageGates.erase(path) > 0 || removed;
        }
        return removed;
    }

    void clearPageGates() {
        _pageGates.clear();
        _wildcardPageGates.clear();
        _asyncPageGates.clear();
        _wildcardAsyncPageGates.clear();
    }

    std::optional<PageGateRule> findMatchingPageGate(const std::string& path) const {
        return findMatchingGateImpl(_pageGates, _wildcardPageGates, path);
    }

    std::optional<AsyncPageGateRule> findMatchingAsyncPageGate(const std::string& path) const {
        return findMatchingGateImpl(_asyncPageGates, _wildcardAsyncPageGates, path);
    }

    /** Best matching sync or async page gate (exact beats wildcard; longest wildcard wins). */
    std::optional<ResolvedPageGate> findResolvedPageGate(const std::string& path) const {
        if (auto resolved = resolveExactPageGate(path)) {
            return resolved;
        }

        if (auto resolved = findBestWildcardPageGate(path)) {
            return resolved;
        }

        const auto strippedPath = stripSupportedLanguagePrefix(path);
        if (!strippedPath.has_value()) {
            return std::nullopt;
        }

        if (auto resolved = resolveExactPageGate(*strippedPath)) {
            return resolved;
        }

        return findBestWildcardPageGate(*strippedPath);
    }

    /** Resolve redirect target for a denied page gate (language-aware when languages are configured). */
    std::string resolvePageGateRedirect(const std::string& redirectTo, const std::string& requestPath) const {
        if (redirectTo.empty()) {
            if (hasLanguages()) {
                return normalizeRedirectTargetLanguage("/", requestPath);
            }
            return defaultPageGateRedirect(requestPath);
        }
        return normalizeRedirectTargetLanguage(redirectTo, requestPath);
    }

    bool addRouteGate(const std::string& path, RouteGateHandler handler) {
        if (path.empty() || !handler) {
            return false;
        }
        storeGateRule(path, RouteGateRule{std::move(handler)}, _routeGates, _wildcardRouteGates);
        return true;
    }

    bool removeRouteGate(const std::string& path) {
        if (path.find('*') != std::string::npos) {
            return _wildcardRouteGates.erase(path) > 0;
        }
        return _routeGates.erase(path) > 0;
    }

    void clearRouteGates() {
        _routeGates.clear();
        _wildcardRouteGates.clear();
    }

    std::optional<RouteGateRule> findMatchingRouteGate(const std::string& path) const {
        return findMatchingGateImpl(_routeGates, _wildcardRouteGates, path);
    }

    /**
     * Find a matching route for the given path, supporting wildcard patterns
     * @param path The requested path
     * @return std::optional<RouteHandler> containing the handler if found, std::nullopt otherwise
     */
    std::optional<RouteHandler> findMatchingRoute(const std::string& path) const {
        // First try exact match for performance (O(1) lookup)
        auto exactMatch = _routes.find(path);
        if (exactMatch != _routes.end()) {
            return exactMatch->second;
        }

        // If no exact match, try wildcard patterns (O(n) lookup)
        for (const auto& route : _wildcardRoutes) {
            if (matchesWildcardPattern(route.first, path)) {
                return route.second;
            }
        }

        return std::nullopt;
    }

    std::optional<AsyncRouteHandler> findMatchingAsyncRoute(const std::string& path) const {
        auto exactMatch = _asyncRoutes.find(path);
        if (exactMatch != _asyncRoutes.end()) {
            return exactMatch->second;
        }
        for (const auto& route : _asyncWildcardRoutes) {
            if (matchesWildcardPattern(route.first, path)) {
                return route.second;
            }
        }
        return std::nullopt;
    }

    std::optional<WebSocketHandler> findMatchingWebSocketRoute(const std::string& path) const {
        auto exactMatch = _webSocketRoutes.find(path);
        if (exactMatch != _webSocketRoutes.end()) {
            return exactMatch->second;
        }
        for (const auto& route : _webSocketWildcardRoutes) {
            if (matchesWildcardPattern(route.first, path)) {
                return route.second;
            }
        }
        return std::nullopt;
    }

    void setWebSocketMaxMessageBytes(size_t bytes) { _webSocketLimits.maxMessageBytes = bytes; }
    void setWebSocketMaxFrameBytes(size_t bytes) { _webSocketLimits.maxFrameBytes = bytes; }
    void setWebSocketIdleTimeout(std::chrono::seconds seconds) { _webSocketLimits.idleTimeout = seconds; }
    void setWebSocketPingInterval(std::chrono::seconds seconds) { _webSocketLimits.pingInterval = seconds; }
    void addWebSocketSubprotocol(std::string name) { _webSocketSubprotocols.push_back(std::move(name)); }

    const WebSocketLimits& getWebSocketLimits() const { return _webSocketLimits; }
    const std::vector<std::string>& getWebSocketSubprotocols() const { return _webSocketSubprotocols; }

    const std::string& getRoot() const { return _root; }
    void setRoot(const std::string& newRoot) { _root = newRoot; }

    bool getRemoveComments() const { return _removeComments; }
    void setRemoveComments(bool value) { _removeComments = value; }

    void keepComments() { _removeComments = false; }

    /**
     * Enable/disable automatic CSS/JS asset merging per page
     * When enabled, HTMLBuilder will scan HTML for <link> and <script> tags,
     * merge all referenced CSS/JS files into single files (page_name.css/js),
     * and replace the original tags with single includes.
     * @param value true to enable merging, false to disable (default: false)
     */
    void setMergeAssets(bool value) { _mergeAssets = value; }

    /**
     * Check if automatic asset merging is enabled
     * @return true if asset merging is enabled
     */
    bool getMergeAssets() const { return _mergeAssets; }

    /**
     * Enable/disable automatic PNG/JPG to WebP conversion
     * When enabled, HTMLBuilder will:
     * - Scan HTML for img tags and CSS url() references with .png/.jpg/.jpeg
     * - Convert referenced images to WebP format
     * - Replace the original references with .webp extensions
     * In devMode, images are converted on-the-fly and cached in memory.
     * In production mode, converted images are saved to disk.
     * @param value true to enable WebP conversion, false to disable (default: false)
     */
    void setWebPConversion(bool value) { _webpConversion = value; }

    /**
     * Check if automatic WebP conversion is enabled
     * @return true if WebP conversion is enabled
     */
    bool getWebPConversion() const { return _webpConversion; }

    /**
     * Enable automatic WebP conversion (alias for setWebPConversion(true))
     * This follows the same pattern as enableDevMode()
     */
    void enableWebPConversion() { _webpConversion = true; }

    /**
     * Set WebP encoding quality
     * @param quality Quality value from 0-100 (default: 75)
     *        Higher values = better quality but larger file size
     *        Lower values = smaller files but lower quality
     *        Recommended: 70-85 for most use cases
     */
    void setWebPQuality(float quality) {
        _webpQuality = quality;
        if (_webpQuality < 0.0f) _webpQuality = 0.0f;
        if (_webpQuality > 100.0f) _webpQuality = 100.0f;
    }

    /**
     * Get WebP encoding quality
     * @return Current quality setting (0-100)
     */
    float getWebPQuality() const { return _webpQuality; }

    /**
     * Set maximum number of HTTP requests handled per keep-alive connection.
     * @param value 0 means unlimited, otherwise exact request cap per connection.
     */
    void setMaxRequestsPerConnection(size_t value) { _maxRequestsPerConnection = value; }

    /**
     * Get maximum number of requests per keep-alive connection.
     * @return 0 for unlimited, otherwise configured cap.
     */
    size_t getMaxRequestsPerConnection() const { return _maxRequestsPerConnection; }

    /**
     * Max size in bytes of a single cached serialized text response (headers + body).
     * Set to 0 to disable storing entries (cache effectively off for new inserts).
     */
    void setTextResponseCacheMaxEntryBytes(size_t bytes) { _textResponseCacheMaxEntryBytes = bytes; }
    size_t getTextResponseCacheMaxEntryBytes() const { return _textResponseCacheMaxEntryBytes; }

    /**
     * Max combined payload bytes for the text response cache across all keys.
     * Set to 0 to disable storing entries (cache effectively off for new inserts).
     */
    void setTextResponseCacheMaxTotalBytes(size_t bytes) { _textResponseCacheMaxTotalBytes = bytes; }
    size_t getTextResponseCacheMaxTotalBytes() const { return _textResponseCacheMaxTotalBytes; }

    /**
     * Enable development mode
     * When enabled:
     * - Log level is automatically set to Debug (all logs shown)
     * - Files are generated in-memory only (not saved to disk)
     * - Text response cache disabled (html/js/css rebuilt every request)
     * - Comments are kept (easier debugging)
     * - Asset merging setting is preserved (can be enabled or disabled separately)
     * This is useful during development when HTML/CSS/JS change frequently
     * @note Should be disabled in production for better performance
     */
    void enableDevMode() {
        _devMode = true;
        setLogLevel(LogLevel::Debug);  // Show all logs
        _removeComments = false;       // Keep comments for easier debugging
        _textResponseCacheMaxEntryBytes = 0;
        _textResponseCacheMaxTotalBytes = 0;
        // Note: Asset merging is NOT disabled - user can control it separately
    }

    /**
     * Check if development mode is enabled
     * @return true if dev mode is active
     */
    bool isDevMode() const { return _devMode; }

    /**
     * Set available languages for the server
     * @param languages Vector of language codes (e.g., {"en", "de", "fr"})
     * The first language in the vector will be used as the default
     */
    void setAvailableLanguages(const std::vector<std::string>& languages) {
        _availableLanguages = languages;
        if (!languages.empty()) {
            _defaultLanguage = languages[0];
        }
    }

    /**
     * Get list of available languages
     * @return Vector of language codes
     */
    const std::vector<std::string>& getAvailableLanguages() const { return _availableLanguages; }

    /**
     * Get the default language (first in the available languages list)
     * @return Default language code, or empty string if no languages configured
     */
    const std::string& getDefaultLanguage() const { return _defaultLanguage; }

    /**
     * Check if a language is available
     * @param lang Language code to check
     * @return true if the language is in the available languages list
     */
    bool isLanguageAvailable(const std::string& lang) const {
        return std::find(_availableLanguages.begin(), _availableLanguages.end(), lang) != _availableLanguages.end();
    }

    /**
     * Check if language system is enabled
     * @return true if at least one language is configured
     */
    bool hasLanguages() const { return !_availableLanguages.empty(); }

    /**
     * Set custom page path for 404 responses (e.g. "/404.html")
     */
    void setNotFoundPage(const std::string& path) { _notFoundPage = path; }

    /**
     * Get configured custom 404 page path
     */
    const std::string& getNotFoundPage() const { return _notFoundPage; }

    /**
     * Check if a custom 404 page is configured
     */
    bool hasNotFoundPage() const { return !_notFoundPage.empty(); }

    /**
     * Get Basic Authentication manager
     * @return Reference to BasicAuth instance
     */
    BasicAuth& getBasicAuth() { return _basicAuth; }
    
    /**
     * Get Basic Authentication manager (const)
     * @return Const reference to BasicAuth instance
     */
    const BasicAuth& getBasicAuth() const { return _basicAuth; }

    /**
     * Set the log level for filtering log output
     * @param level LogLevel enum value (None, Error, Warning, Info, Debug)
     * @note Thread-safe - can be called from any thread at runtime
     */
    void setLogLevel(LogLevel level) { _logLevel.store(level, std::memory_order_relaxed); }

    /**
     * Get the current log level
     * @return Current LogLevel
     * @note Thread-safe
     */
    LogLevel getLogLevel() const { return _logLevel.load(std::memory_order_relaxed); }

    /**
     * Check if a message at the given level should be logged
     * @param level The level of the message to check
     * @return true if the message should be logged based on current log level
     * @note Thread-safe - called from multiple handler threads
     */
    bool shouldLog(LogLevel level) const {
        return static_cast<int>(level) <= static_cast<int>(_logLevel.load(std::memory_order_relaxed));
    }

    void setDatabaseClient(std::shared_ptr<db::DatabaseClient> client) { _databaseClient = std::move(client); }
    std::shared_ptr<db::DatabaseClient> getDatabaseClient() const { return _databaseClient; }

    /**
     * Set JavaScript obfuscation level
     * @param level 0=disabled (default), 1=basic, 2=medium, 3=advanced
     * @note Only applies when dev mode is off
     */
    void setObfuscationLevel(unsigned int level) { _obfuscationLevel = level; }

    /**
     * Get current obfuscation level
     * @return Current obfuscation level (0-3)
     */
    unsigned int getObfuscationLevel() const { return _obfuscationLevel; }

    /**
     * Set cache expiry time for obfuscated files
     * @param days Number of days to keep obfuscated files (default: 7)
     */
    void setObfuscationCacheExpiry(int days) { _obfuscationCacheExpiryDays = days; }

    /**
     * Get cache expiry time
     * @return Number of days before cached obfuscated files expire
     */
    int getObfuscationCacheExpiry() const { return _obfuscationCacheExpiryDays; }

    /**
     * Add a file to obfuscation exclusion list
     * Files in this list will not be obfuscated or merged
     * @param filename Exact filename to exclude (e.g., "jquery.min.js")
     */
    void addObfuscationExclusion(const std::string& filename) {
        _obfuscationExclusions.push_back(filename);
    }

    /**
     * Check if a file is excluded from obfuscation
     * @param filename Filename to check
     * @return true if file is in exclusion list
     */
    bool isObfuscationExcluded(const std::string& filename) const {
        for (const auto& excluded : _obfuscationExclusions) {
            if (filename == excluded || filename.find(excluded) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    /**
     * Get all obfuscation exclusions
     * @return Vector of excluded filenames
     */
    const std::vector<std::string>& getObfuscationExclusions() const {
        return _obfuscationExclusions;
    }

    void addObfuscationPreserveIdent(const std::string& name) {
        if (!name.empty()) {
            _obfuscationPreserveIdents.insert(name);
        }
    }

    void addObfuscationExternGlobal(const std::string& name) {
        if (!name.empty()) {
            _obfuscationExternGlobals.insert(name);
        }
    }

    const std::unordered_set<std::string>& getObfuscationPreserveIdents() const {
        return _obfuscationPreserveIdents;
    }

    const std::unordered_set<std::string>& getObfuscationExternGlobals() const {
        return _obfuscationExternGlobals;
    }

    /**
     * One name per line; empty lines and lines starting with # ignored.
     */
    void loadObfuscationExternsFromText(const std::string& text) {
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) {
                end = text.size();
            }
            std::string line = text.substr(pos, end - pos);
            size_t a = 0;
            while (a < line.size()
                   && std::isspace(static_cast<unsigned char>(line[a]))) {
                ++a;
            }
            size_t b = line.size();
            while (b > a && std::isspace(static_cast<unsigned char>(line[b - 1]))) {
                --b;
            }
            line = line.substr(a, b - a);
            if (!line.empty() && line[0] != '#') {
                addObfuscationExternGlobal(line);
            }
            pos = end + 1;
        }
    }

    void setObfuscationStrictUndefined(bool v) { _obfuscationStrictUndefined = v; }

    bool getObfuscationStrictUndefined() const { return _obfuscationStrictUndefined; }

    void setObfuscationEmitGlobalThisAssignments(bool v) { _obfuscationEmitGlobalThisBracket = v; }

    bool getObfuscationEmitGlobalThisAssignments() const { return _obfuscationEmitGlobalThisBracket; }

    void setObfuscationValidateWithAcorn(bool v) { _obfuscationValidateWithAcorn = v; }

    bool getObfuscationValidateWithAcorn() const { return _obfuscationValidateWithAcorn; }

    void setObfuscationAutoBracketKeys(bool v) { _obfuscationAutoBracketKeys = v; }
    bool getObfuscationAutoBracketKeys() const { return _obfuscationAutoBracketKeys; }

    /**
     * Check if obfuscation should be applied
     * Returns false if dev mode is on or obfuscation level is 0
     * @return true if obfuscation should be applied
     */
    bool shouldObfuscate() const {
        return !_devMode && _obfuscationLevel > 0;
    }

    // ========== Metrics Methods ==========

    /** Built-in monitoring paths (e.g. /status) must not inflate request counters. */
    static bool isMetricsExcludedPath(const std::string& path) {
        return path == "/status";
    }

    void recordRequest() const {
        _totalRequests.fetch_add(1, std::memory_order_relaxed);
        const auto ep = _nowEpochs();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _writeBuckets(ep.first, ep.second, 1,0,0,0, 0.f,0);
    }

    void recordError() const {
        _totalInternalErrors.fetch_add(1, std::memory_order_relaxed);
        const auto ep = _nowEpochs();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _writeBuckets(ep.first, ep.second, 0,0,0,1, 0.f,0);
    }

    void record4xx() const {
        _total4xx.fetch_add(1, std::memory_order_relaxed);
        const auto ep = _nowEpochs();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _writeBuckets(ep.first, ep.second, 0,1,0,0, 0.f,0);
    }

    void record5xx() const {
        _total5xx.fetch_add(1, std::memory_order_relaxed);
        const auto ep = _nowEpochs();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _writeBuckets(ep.first, ep.second, 0,0,1,0, 0.f,0);
    }

    void recordQueueRejection() const {
        _queueRejections.fetch_add(1, std::memory_order_relaxed);
    }

    void recordAcceptError() const {
        _acceptErrorsTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void recordAcceptEmfile() const {
        _acceptEmfileTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void recordFileOpenFailure() const {
        _fileOpenFailures.fetch_add(1, std::memory_order_relaxed);
    }

    void recordOverloadHttpResponse() const {
        _overloadHttpResponses.fetch_add(1, std::memory_order_relaxed);
    }

    void recordQueueFill(float fillPct) const {
        const auto ep = _nowEpochs();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _writeBuckets(ep.first, ep.second, 0,0,0,0, fillPct, 1);
    }

    void incrementActiveHandlers() const {
        _activeHandlers.fetch_add(1, std::memory_order_relaxed);
    }

    void decrementActiveHandlers() const {
        _activeHandlers.fetch_sub(1, std::memory_order_relaxed);
    }

    void recordLatency(uint32_t us) const {
        const auto ep = _nowEpochs();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _latSamples[_latHead] = {ep.first, us};
        _latHead = (_latHead + 1) % _LAT_CAP;
        if (_latCount < _LAT_CAP) ++_latCount;
    }

    // ========== Getters ==========

    uint64_t getTotalRequests() const {
        return _totalRequests.load(std::memory_order_relaxed);
    }

    uint64_t getTotalErrors() const {
        return _total4xx.load(std::memory_order_relaxed)
             + _total5xx.load(std::memory_order_relaxed)
             + _totalInternalErrors.load(std::memory_order_relaxed);
    }

    uint64_t getTotal4xx() const { return _total4xx.load(std::memory_order_relaxed); }
    uint64_t getTotal5xx() const { return _total5xx.load(std::memory_order_relaxed); }
    uint64_t getTotalInternalErrors() const { return _totalInternalErrors.load(std::memory_order_relaxed); }
    uint64_t getQueueRejections() const { return _queueRejections.load(std::memory_order_relaxed); }
    uint64_t getAcceptErrorsTotal() const { return _acceptErrorsTotal.load(std::memory_order_relaxed); }
    uint64_t getAcceptEmfileTotal() const { return _acceptEmfileTotal.load(std::memory_order_relaxed); }
    uint64_t getFileOpenFailures() const { return _fileOpenFailures.load(std::memory_order_relaxed); }
    uint64_t getOverloadHttpResponses() const { return _overloadHttpResponses.load(std::memory_order_relaxed); }
    int64_t  getActiveHandlers() const { return _activeHandlers.load(std::memory_order_relaxed); }

    struct WindowMetrics {
        uint64_t requests       = 0;
        uint64_t errors_4xx     = 0;
        uint64_t errors_5xx     = 0;
        uint64_t errors_int     = 0;
        double   avg_queue_fill = 0.0;
    };
    WindowMetrics getWindowMetricsHour() const {
        const auto ep = _nowEpochs();
        const uint32_t curM = ep.second;
        std::lock_guard<std::mutex> lock(_metricsMutex);
        WindowMetrics wm;
        double fillSum = 0.0; uint64_t fillN = 0;
        for (const auto& b : _minBuckets) {
            if (b.epoch > 0 && curM >= b.epoch && (curM - b.epoch) < 60) {
                wm.requests   += b.requests;
                wm.errors_4xx += b.errors_4xx;
                wm.errors_5xx += b.errors_5xx;
                wm.errors_int += b.errors_int;
                fillSum += b.fill_sum; fillN += b.fill_count;
            }
        }
        if (fillN > 0) wm.avg_queue_fill = fillSum / static_cast<double>(fillN);
        return wm;
    }

    // Rolling average per hour since restart
    WindowMetrics getRollingAveragePerHour() const {
        const uint64_t uptime = getUptimeSeconds();
        const uint64_t hours = uptime / 3600;
        if (hours == 0) return getWindowMetricsHour();
        std::lock_guard<std::mutex> lock(_metricsMutex);
        WindowMetrics wm;
        double fillSum = 0.0; uint64_t fillN = 0;
        for (const auto& b : _minBuckets) {
            wm.requests   += b.requests;
            wm.errors_4xx += b.errors_4xx;
            wm.errors_5xx += b.errors_5xx;
            wm.errors_int += b.errors_int;
            fillSum += b.fill_sum; fillN += b.fill_count;
        }
        if (fillN > 0) wm.avg_queue_fill = fillSum / static_cast<double>(fillN);
        // Average per hour
        wm.requests   = hours ? wm.requests / hours : wm.requests;
        wm.errors_4xx = hours ? wm.errors_4xx / hours : wm.errors_4xx;
        wm.errors_5xx = hours ? wm.errors_5xx / hours : wm.errors_5xx;
        wm.errors_int = hours ? wm.errors_int / hours : wm.errors_int;
        return wm;
    }

    struct LatencyStats { double p50 = 0.0; double p95 = 0.0; double p99 = 0.0; };

    LatencyStats getLatencyStats(uint32_t windowSeconds) const {
        const auto ep = _nowEpochs();
        const uint32_t curS = ep.first;
        const uint32_t cutoff = (curS > windowSeconds) ? (curS - windowSeconds) : 0;
        std::lock_guard<std::mutex> lock(_metricsMutex);
        std::vector<uint32_t> relevant;
        relevant.reserve(_latCount);
        const size_t start = (_latHead + _LAT_CAP - _latCount) % _LAT_CAP;
        for (size_t i = 0; i < _latCount; ++i) {
            const LatencySample& s = _latSamples[(start + i) % _LAT_CAP];
            if (s.epoch_s >= cutoff) relevant.push_back(s.us);
        }
        if (relevant.empty()) return {};
        std::sort(relevant.begin(), relevant.end());
        const size_t n = relevant.size();
        return {
            relevant[n * 50 / 100] / 1000.0,
            relevant[n * 95 / 100] / 1000.0,
            relevant[n * 99 / 100] / 1000.0
        };
    }

    uint64_t getUptimeSeconds() const {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - _startTime).count());
    }

    /**
     * Load persisted metrics from JSON (schema v1). Missing file is OK. Resets session start time after load.
     * @return false if the file existed but was invalid; metrics may be partially applied or cleared.
     */
    bool loadPersistentMetricsFromFile(const std::string& path);

    /**
     * Atomically snapshot metrics to JSON (schema v1), including lifetime uptime through this moment.
     */
    bool savePersistentMetricsToFile(const std::string& path) const;

    /** Cumulative uptime in hours: baseline from disk plus current session (since last load in start()). */
    double getUptimeHoursTotal() const {
        return static_cast<double>(_lifetimeUptimeBaselineSeconds + getUptimeSeconds()) / 3600.0;
    }

   private:
    void clearAllMetrics_();

    /// @param root copied so JSONParser getters (non-const) can be used during import
    bool importPersistentMetricsJson_(JSONParser root);
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP