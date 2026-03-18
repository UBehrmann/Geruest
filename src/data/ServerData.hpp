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
#include <atomic>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "../auth/BasicAuth.hpp"

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

using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;

// class with the server data
class ServerData {
   private:
    struct RedirectRule {
        std::string target;
        int status = 301;
    };

    std::unordered_map<std::string, RouteHandler> _routes;
    std::unordered_map<std::string, RouteHandler> _wildcardRoutes;
    std::unordered_map<std::string, RedirectRule> _redirects;
    std::unordered_map<std::string, RedirectRule> _wildcardRedirects;
    std::string _root;
    bool _removeComments = true;    // Remove comments from built files
    bool _mergeAssets = false;      // Automatic CSS/JS merging per page
    bool _devMode = false;          // Development mode (no file caching, verbose logging)
    bool _webpConversion = false;   // Automatic PNG/JPG to WebP conversion
    float _webpQuality = 75.0f;     // WebP encoding quality (0-100, default 75%)
    unsigned int _obfuscationLevel = 0;  // JS obfuscation level (0=disabled, 1-3=increasing complexity)
    int _obfuscationCacheExpiryDays = 7;  // Days to keep obfuscated files cached
    std::vector<std::string> _obfuscationExclusions;  // Files excluded from obfuscation and merging
    std::vector<std::string> _availableLanguages;
    std::string _defaultLanguage;
    std::string _notFoundPage;
    BasicAuth _basicAuth;
    std::atomic<LogLevel> _logLevel{LogLevel::Error};  // Thread-safe log level (can be changed at runtime)

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

    std::optional<std::pair<std::string, int>> findMatchingWildcardRedirect(const std::string& path) const {
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

            const std::string resolvedTarget = applyWildcardCapture(redirect.second.target, *capture);
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

   public:
    ServerData() = default;

    // Custom copy constructor needed because std::atomic is not copyable
    ServerData(const ServerData& other)
        : _routes(other._routes),
          _wildcardRoutes(other._wildcardRoutes),
                    _redirects(other._redirects),
                    _wildcardRedirects(other._wildcardRedirects),
          _root(other._root),
          _removeComments(other._removeComments),
          _mergeAssets(other._mergeAssets),
          _devMode(other._devMode),
          _webpConversion(other._webpConversion),
          _webpQuality(other._webpQuality),
          _availableLanguages(other._availableLanguages),
          _defaultLanguage(other._defaultLanguage),
          _notFoundPage(other._notFoundPage),
          _basicAuth(other._basicAuth),
          _logLevel(other._logLevel.load(std::memory_order_relaxed)) {}

    // Custom copy assignment operator needed because std::atomic is not copyable
    ServerData& operator=(const ServerData& other) {
        if (this != &other) {
            _routes = other._routes;
            _wildcardRoutes = other._wildcardRoutes;
            _redirects = other._redirects;
            _wildcardRedirects = other._wildcardRedirects;
            _root = other._root;
            _removeComments = other._removeComments;
            _mergeAssets = other._mergeAssets;
            _devMode = other._devMode;
            _webpConversion = other._webpConversion;
            _webpQuality = other._webpQuality;
            _availableLanguages = other._availableLanguages;
            _defaultLanguage = other._defaultLanguage;
            _notFoundPage = other._notFoundPage;
            _basicAuth = other._basicAuth;
            _logLevel.store(other._logLevel.load(std::memory_order_relaxed), std::memory_order_relaxed);
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
            return std::make_pair(exactMatch->second.target, exactMatch->second.status);
        }

        // Priority rule 2: wildcard redirect after exact redirect
        return findMatchingWildcardRedirect(path);
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
     * Enable development mode
     * When enabled:
     * - Log level is automatically set to Debug (all logs shown)
     * - Files are generated in-memory only (not saved to disk)
     * - Comments are kept (easier debugging)
     * - Asset merging setting is preserved (can be enabled or disabled separately)
     * This is useful during development when HTML/CSS/JS change frequently
     * @note Should be disabled in production for better performance
     */
    void enableDevMode() {
        _devMode = true;
        setLogLevel(LogLevel::Debug);  // Show all logs
        _removeComments = false;       // Keep comments for easier debugging
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

    /**
     * Check if obfuscation should be applied
     * Returns false if dev mode is on or obfuscation level is 0
     * @return true if obfuscation should be applied
     */
    bool shouldObfuscate() const {
        return !_devMode && _obfuscationLevel > 0;
    }
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP