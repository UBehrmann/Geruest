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
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

namespace geruest {

using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;

// class with the server data
class ServerData {
   private:
    std::unordered_map<std::string, RouteHandler> _routes;
    std::unordered_map<std::string, RouteHandler> _wildcardRoutes;
    std::string _root;
    bool _removeComments = true;
    std::vector<std::string> _availableLanguages;
    std::string _defaultLanguage;

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

   public:
    ServerData() = default;

    ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string root)
        : _routes(routes), _root(std::move(root)) {}

    std::unordered_map<std::string, RouteHandler>& getRoutes() {
        // For backward compatibility, merge both maps
        // Note: This creates a temporary merged map which may impact performance
        // Consider deprecating this method in favor of the new findMatchingRoute
        static std::unordered_map<std::string, RouteHandler> merged;
        merged.clear();
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

    /**
     * Find a matching route for the given path, supporting wildcard patterns
     * @param path The requested path
     * @return Pair of <found, RouteHandler>. found is true if a match was found
     */
    std::pair<bool, RouteHandler> findMatchingRoute(const std::string& path) const {
        // First try exact match for performance (O(1) lookup)
        auto exactMatch = _routes.find(path);
        if (exactMatch != _routes.end()) {
            return {true, exactMatch->second};
        }

        // If no exact match, try wildcard patterns (O(n) lookup)
        for (const auto& route : _wildcardRoutes) {
            if (matchesWildcardPattern(route.first, path)) {
                return {true, route.second};
            }
        }

        return {false, RouteHandler{}};
    }

    std::string getRoot() const { return _root; }
    void setRoot(const std::string& newRoot) { _root = newRoot; }

    bool getRemoveComments() const { return _removeComments; }
    void setRemoveComments(bool value) { _removeComments = value; }

    void keepComments() { _removeComments = false; }

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
    std::string getDefaultLanguage() const { return _defaultLanguage; }

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
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP