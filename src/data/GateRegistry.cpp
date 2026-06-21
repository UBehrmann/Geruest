#include "GateRegistry.hpp"

#include "WildcardMatch.hpp"

namespace geruest {

template <typename Rule>
void GateRegistry::storeGateRule(const std::string& path, Rule rule,
                                 std::unordered_map<std::string, Rule>& exactGates,
                                 std::unordered_map<std::string, Rule>& wildcardGates) {
    if (path.find('*') != std::string::npos) {
        wildcardGates[path] = std::move(rule);
    } else {
        exactGates[path] = std::move(rule);
    }
}

template <typename Rule>
std::optional<Rule> GateRegistry::findMatchingGateImpl(const std::unordered_map<std::string, Rule>& exactGates,
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
                            (pattern.size() == bestPatternLength && bestPattern.has_value() && pattern < *bestPattern);
        if (better) {
            bestPatternLength = pattern.size();
            bestPattern = pattern;
            bestMatch = gate.second;
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
                            (pattern.size() == bestPatternLength && bestPattern.has_value() && pattern < *bestPattern);
        if (better) {
            bestPatternLength = pattern.size();
            bestPattern = pattern;
            bestMatch = gate.second;
        }
    }
    return bestMatch;
}

template <typename Resolved, typename AsyncRule, typename SyncRule, typename FromAsync, typename FromSync>
std::optional<Resolved> GateRegistry::findBestWildcardGate(
    const std::string& path, const std::unordered_map<std::string, AsyncRule>& asyncWild,
    const std::unordered_map<std::string, SyncRule>& syncWild, FromAsync&& fromAsync, FromSync&& fromSync) const {
    size_t bestPatternLength = 0;
    std::optional<std::string> bestPattern;
    std::optional<Resolved> bestMatch;

    auto consider = [&](const std::string& pattern, Resolved resolved) {
        const bool better = !bestMatch.has_value() || pattern.size() > bestPatternLength ||
                            (pattern.size() == bestPatternLength && bestPattern.has_value() && pattern < *bestPattern);
        if (better) {
            bestPatternLength = pattern.size();
            bestPattern = pattern;
            bestMatch = std::move(resolved);
        }
    };

    for (const auto& gate : asyncWild) {
        const std::string& pattern = gate.first;
        if (matchesWildcardPattern(pattern, path)) {
            consider(pattern, fromAsync(gate.second));
        }
    }
    for (const auto& gate : syncWild) {
        const std::string& pattern = gate.first;
        if (matchesWildcardPattern(pattern, path)) {
            consider(pattern, fromSync(gate.second));
        }
    }

    return bestMatch;
}

bool GateRegistry::addPageGate(const std::string& path, PageGateHandler handler, const std::string& redirectTo) {
    if (path.empty() || !handler) {
        return false;
    }
    storeGateRule(path, PageGateRule{std::move(handler), redirectTo}, _pageGates, _wildcardPageGates);
    return true;
}

bool GateRegistry::addAsyncPageGate(const std::string& path, AsyncPageGateHandler handler,
                                    const std::string& redirectTo) {
    if (path.empty() || !handler) {
        return false;
    }
    storeGateRule(path, AsyncPageGateRule{std::move(handler), redirectTo}, _asyncPageGates, _wildcardAsyncPageGates);
    return true;
}

bool GateRegistry::removePageGate(const std::string& path) {
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

void GateRegistry::clearPageGates() {
    _pageGates.clear();
    _wildcardPageGates.clear();
    _asyncPageGates.clear();
    _wildcardAsyncPageGates.clear();
}

std::optional<PageGateRule> GateRegistry::findMatchingPageGate(const std::string& path) const {
    return findMatchingGateImpl(_pageGates, _wildcardPageGates, canonicalRequestPath(path));
}

std::optional<AsyncPageGateRule> GateRegistry::findMatchingAsyncPageGate(const std::string& path) const {
    return findMatchingGateImpl(_asyncPageGates, _wildcardAsyncPageGates, canonicalRequestPath(path));
}

std::optional<ResolvedPageGate> GateRegistry::resolveExactPageGate(const std::string& lookupPath) const {
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

std::optional<ResolvedPageGate> GateRegistry::findBestWildcardPageGate(const std::string& path) const {
    return findBestWildcardGate<ResolvedPageGate>(
        path, _wildcardAsyncPageGates, _wildcardPageGates,
        [](const AsyncPageGateRule& rule) { return ResolvedPageGate{true, {}, rule.handler, rule.redirectTo}; },
        [](const PageGateRule& rule) { return ResolvedPageGate{false, rule.handler, {}, rule.redirectTo}; });
}

std::optional<ResolvedPageGate> GateRegistry::findResolvedPageGate(const std::string& path) const {
    const std::string canon = canonicalRequestPath(path);
    if (auto resolved = resolveExactPageGate(canon)) {
        return resolved;
    }

    if (auto resolved = findBestWildcardPageGate(canon)) {
        return resolved;
    }

    if (_languages == nullptr) {
        return std::nullopt;
    }

    const auto strippedPath = _languages->stripSupportedLanguagePrefix(canon);
    if (!strippedPath.has_value()) {
        return std::nullopt;
    }

    if (auto resolved = resolveExactPageGate(*strippedPath)) {
        return resolved;
    }

    return findBestWildcardPageGate(*strippedPath);
}

std::string GateRegistry::resolvePageGateRedirect(const std::string& redirectTo,
                                                  const std::string& requestPath) const {
    if (redirectTo.empty()) {
        if (_languages != nullptr && _languages->hasLanguages()) {
            return _languages->normalizeRedirectTargetLanguage("/", requestPath);
        }
        return defaultPageGateRedirect(requestPath);
    }
    if (_languages != nullptr) {
        return _languages->normalizeRedirectTargetLanguage(redirectTo, requestPath);
    }
    return redirectTo;
}

bool GateRegistry::addRouteGate(const std::string& path, RouteGateHandler handler) {
    if (path.empty() || !handler) {
        return false;
    }
    storeGateRule(path, RouteGateRule{std::move(handler)}, _routeGates, _wildcardRouteGates);
    return true;
}

bool GateRegistry::addAsyncRouteGate(const std::string& path, AsyncRouteGateHandler handler) {
    if (path.empty() || !handler) {
        return false;
    }
    storeGateRule(path, AsyncRouteGateRule{std::move(handler)}, _asyncRouteGates, _wildcardAsyncRouteGates);
    return true;
}

bool GateRegistry::removeRouteGate(const std::string& path) {
    bool removed = false;
    if (path.find('*') != std::string::npos) {
        removed = _wildcardRouteGates.erase(path) > 0;
        removed = _wildcardAsyncRouteGates.erase(path) > 0 || removed;
    } else {
        removed = _routeGates.erase(path) > 0;
        removed = _asyncRouteGates.erase(path) > 0 || removed;
    }
    return removed;
}

void GateRegistry::clearRouteGates() {
    _routeGates.clear();
    _wildcardRouteGates.clear();
    _asyncRouteGates.clear();
    _wildcardAsyncRouteGates.clear();
}

std::optional<RouteGateRule> GateRegistry::findMatchingRouteGate(const std::string& path) const {
    return findMatchingGateImpl(_routeGates, _wildcardRouteGates, canonicalRequestPath(path));
}

std::optional<ResolvedRouteGate> GateRegistry::resolveExactRouteGate(const std::string& lookupPath) const {
    const auto asyncIt = _asyncRouteGates.find(lookupPath);
    if (asyncIt != _asyncRouteGates.end()) {
        return ResolvedRouteGate{true, {}, asyncIt->second.handler};
    }
    const auto syncIt = _routeGates.find(lookupPath);
    if (syncIt != _routeGates.end()) {
        return ResolvedRouteGate{false, syncIt->second.handler, {}};
    }
    return std::nullopt;
}

std::optional<ResolvedRouteGate> GateRegistry::findBestWildcardRouteGate(const std::string& path) const {
    return findBestWildcardGate<ResolvedRouteGate>(
        path, _wildcardAsyncRouteGates, _wildcardRouteGates,
        [](const AsyncRouteGateRule& rule) { return ResolvedRouteGate{true, {}, rule.handler}; },
        [](const RouteGateRule& rule) { return ResolvedRouteGate{false, rule.handler, {}}; });
}

std::optional<ResolvedRouteGate> GateRegistry::findResolvedRouteGate(const std::string& path) const {
    const std::string canon = canonicalRequestPath(path);
    if (auto resolved = resolveExactRouteGate(canon)) {
        return resolved;
    }

    if (auto resolved = findBestWildcardRouteGate(canon)) {
        return resolved;
    }

    if (_languages == nullptr) {
        return std::nullopt;
    }

    const auto strippedPath = _languages->stripSupportedLanguagePrefix(canon);
    if (!strippedPath.has_value()) {
        return std::nullopt;
    }

    if (auto resolved = resolveExactRouteGate(*strippedPath)) {
        return resolved;
    }

    return findBestWildcardRouteGate(*strippedPath);
}

}  // namespace geruest
