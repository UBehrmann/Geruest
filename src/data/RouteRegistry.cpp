#include "RouteRegistry.hpp"

#include "WildcardMatch.hpp"

#include <set>

namespace geruest {

std::unordered_map<std::string, RouteHandler> RouteRegistry::getRoutesMerged() const {
    std::unordered_map<std::string, RouteHandler> merged;
    merged.insert(_routes.begin(), _routes.end());
    merged.insert(_wildcardRoutes.begin(), _wildcardRoutes.end());
    return merged;
}

void RouteRegistry::addRoute(const std::string& path, RouteHandler routeHandler) {
    if (path.find('*') != std::string::npos) {
        _wildcardRoutes[path] = std::move(routeHandler);
    } else {
        _routes[path] = std::move(routeHandler);
    }
}

void RouteRegistry::addRoute(const std::string& path, AsyncRouteHandler routeHandler) {
    if (path.find('*') != std::string::npos) {
        _asyncWildcardRoutes[path] = std::move(routeHandler);
    } else {
        _asyncRoutes[path] = std::move(routeHandler);
    }
}

void RouteRegistry::addWebSocketRoute(const std::string& path, WebSocketHandler routeHandler) {
    if (path.find('*') != std::string::npos) {
        _webSocketWildcardRoutes[path] = std::move(routeHandler);
    } else {
        _webSocketRoutes[path] = std::move(routeHandler);
    }
}

bool RouteRegistry::addRedirect(const std::string& from, const std::string& to, int status) {
    if (from.empty() || to.empty()) {
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

size_t RouteRegistry::addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status) {
    size_t addedCount = 0;
    for (const auto& entry : redirects) {
        if (addRedirect(entry.first, entry.second, status)) {
            ++addedCount;
        }
    }
    return addedCount;
}

template <typename Rule>
std::optional<Rule> RouteRegistry::findMatchingRouteImpl(
    const std::unordered_map<std::string, Rule>& exactRoutes,
    const std::unordered_map<std::string, Rule>& wildcardRoutes, const std::string& path) const {
    auto exactMatch = exactRoutes.find(path);
    if (exactMatch != exactRoutes.end()) {
        return exactMatch->second;
    }

    size_t bestPatternLength = 0;
    std::optional<std::string> bestPattern;
    std::optional<Rule> bestMatch;
    for (const auto& route : wildcardRoutes) {
        const std::string& pattern = route.first;
        if (!matchesWildcardPattern(pattern, path)) {
            continue;
        }
        const bool better = !bestMatch.has_value() || pattern.size() > bestPatternLength ||
                            (pattern.size() == bestPatternLength && bestPattern.has_value() && pattern < *bestPattern);
        if (better) {
            bestPatternLength = pattern.size();
            bestPattern = pattern;
            bestMatch = route.second;
        }
    }
    if (bestMatch.has_value()) {
        return bestMatch;
    }

    if (_languages == nullptr) {
        return std::nullopt;
    }

    const auto strippedPath = _languages->stripSupportedLanguagePrefix(path);
    if (!strippedPath.has_value()) {
        return std::nullopt;
    }

    exactMatch = exactRoutes.find(*strippedPath);
    if (exactMatch != exactRoutes.end()) {
        return exactMatch->second;
    }

    bestPatternLength = 0;
    bestPattern.reset();
    bestMatch.reset();
    for (const auto& route : wildcardRoutes) {
        const std::string& pattern = route.first;
        if (!matchesWildcardPattern(pattern, *strippedPath)) {
            continue;
        }
        const bool better = !bestMatch.has_value() || pattern.size() > bestPatternLength ||
                            (pattern.size() == bestPatternLength && bestPattern.has_value() && pattern < *bestPattern);
        if (better) {
            bestPatternLength = pattern.size();
            bestPattern = pattern;
            bestMatch = route.second;
        }
    }
    return bestMatch;
}

std::optional<std::pair<std::string, int>> RouteRegistry::findMatchingWildcardRedirect(
    const std::string& path, const std::string& requestPath) const {
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
        const std::string resolvedTarget =
            _languages != nullptr ? _languages->normalizeRedirectTargetLanguage(capturedTarget, requestPath)
                                  : capturedTarget;
        if (!bestMatch.has_value() || pattern.size() > bestPatternLength) {
            bestPatternLength = pattern.size();
            bestMatch = std::make_pair(resolvedTarget, redirect.second.status);
        }
    }

    return bestMatch;
}

bool RouteRegistry::hasRedirectLoop(const std::string& startPath) const {
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

std::optional<std::pair<std::string, int>> RouteRegistry::findMatchingRedirect(const std::string& path) const {
    auto exactMatch = _redirects.find(path);
    if (exactMatch != _redirects.end()) {
        const std::string target =
            _languages != nullptr ? _languages->normalizeRedirectTargetLanguage(exactMatch->second.target, path)
                                  : exactMatch->second.target;
        return std::make_pair(target, exactMatch->second.status);
    }

    auto wildcardMatch = findMatchingWildcardRedirect(path, path);
    if (wildcardMatch.has_value()) {
        return wildcardMatch;
    }

    if (_languages == nullptr) {
        return std::nullopt;
    }

    const auto strippedPath = _languages->stripSupportedLanguagePrefix(path);
    if (!strippedPath.has_value()) {
        return std::nullopt;
    }

    exactMatch = _redirects.find(*strippedPath);
    if (exactMatch != _redirects.end()) {
        return std::make_pair(_languages->normalizeRedirectTargetLanguage(exactMatch->second.target, path),
                              exactMatch->second.status);
    }

    return findMatchingWildcardRedirect(*strippedPath, path);
}

std::optional<RouteHandler> RouteRegistry::findMatchingRoute(const std::string& path) const {
    return findMatchingRouteImpl(_routes, _wildcardRoutes, canonicalRequestPath(path));
}

std::optional<AsyncRouteHandler> RouteRegistry::findMatchingAsyncRoute(const std::string& path) const {
    return findMatchingRouteImpl(_asyncRoutes, _asyncWildcardRoutes, canonicalRequestPath(path));
}

std::optional<WebSocketHandler> RouteRegistry::findMatchingWebSocketRoute(const std::string& path) const {
    return findMatchingRouteImpl(_webSocketRoutes, _webSocketWildcardRoutes, canonicalRequestPath(path));
}

// Explicit template instantiations for link
template std::optional<RouteHandler> RouteRegistry::findMatchingRouteImpl(
    const std::unordered_map<std::string, RouteHandler>&, const std::unordered_map<std::string, RouteHandler>&,
    const std::string&) const;
template std::optional<AsyncRouteHandler> RouteRegistry::findMatchingRouteImpl(
    const std::unordered_map<std::string, AsyncRouteHandler>&,
    const std::unordered_map<std::string, AsyncRouteHandler>&, const std::string&) const;
template std::optional<WebSocketHandler> RouteRegistry::findMatchingRouteImpl(
    const std::unordered_map<std::string, WebSocketHandler>&,
    const std::unordered_map<std::string, WebSocketHandler>&, const std::string&) const;

}  // namespace geruest
