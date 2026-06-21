/**
 * @file GateRegistry.hpp
 * @brief Page and route access gates.
 */

#ifndef GERUEST_GATEREGISTRY_HPP
#define GERUEST_GATEREGISTRY_HPP

#include <optional>
#include <string>
#include <unordered_map>

#include "LanguageConfig.hpp"
#include "ServerTypes.hpp"

namespace geruest {

class GateRegistry {
   public:
    GateRegistry() = default;
    explicit GateRegistry(const LanguageConfig* languages) : _languages(languages) {}
    GateRegistry(const GateRegistry&) = default;
    GateRegistry& operator=(const GateRegistry&) = default;

    void setLanguageConfig(const LanguageConfig* languages) { _languages = languages; }

    bool addPageGate(const std::string& path, PageGateHandler handler, const std::string& redirectTo = "");
    bool addAsyncPageGate(const std::string& path, AsyncPageGateHandler handler, const std::string& redirectTo = "");
    bool removePageGate(const std::string& path);
    void clearPageGates();

    std::optional<PageGateRule> findMatchingPageGate(const std::string& path) const;
    std::optional<AsyncPageGateRule> findMatchingAsyncPageGate(const std::string& path) const;
    std::optional<ResolvedPageGate> findResolvedPageGate(const std::string& path) const;
    std::string resolvePageGateRedirect(const std::string& redirectTo, const std::string& requestPath) const;

    bool addRouteGate(const std::string& path, RouteGateHandler handler);
    bool addAsyncRouteGate(const std::string& path, AsyncRouteGateHandler handler);
    bool removeRouteGate(const std::string& path);
    void clearRouteGates();

    std::optional<RouteGateRule> findMatchingRouteGate(const std::string& path) const;
    std::optional<ResolvedRouteGate> findResolvedRouteGate(const std::string& path) const;

   private:
    template <typename Rule>
    void storeGateRule(const std::string& path, Rule rule, std::unordered_map<std::string, Rule>& exactGates,
                       std::unordered_map<std::string, Rule>& wildcardGates);

    template <typename Rule>
    std::optional<Rule> findMatchingGateImpl(const std::unordered_map<std::string, Rule>& exactGates,
                                             const std::unordered_map<std::string, Rule>& wildcardGates,
                                             const std::string& path) const;

    template <typename Resolved, typename AsyncRule, typename SyncRule, typename FromAsync, typename FromSync>
    std::optional<Resolved> findBestWildcardGate(const std::string& path,
                                                 const std::unordered_map<std::string, AsyncRule>& asyncWild,
                                                 const std::unordered_map<std::string, SyncRule>& syncWild,
                                                 FromAsync&& fromAsync, FromSync&& fromSync) const;

    std::optional<ResolvedRouteGate> findBestWildcardRouteGate(const std::string& path) const;
    std::optional<ResolvedRouteGate> resolveExactRouteGate(const std::string& lookupPath) const;
    std::optional<ResolvedPageGate> findBestWildcardPageGate(const std::string& path) const;
    std::optional<ResolvedPageGate> resolveExactPageGate(const std::string& lookupPath) const;

    const LanguageConfig* _languages = nullptr;

    std::unordered_map<std::string, PageGateRule> _pageGates;
    std::unordered_map<std::string, PageGateRule> _wildcardPageGates;
    std::unordered_map<std::string, AsyncPageGateRule> _asyncPageGates;
    std::unordered_map<std::string, AsyncPageGateRule> _wildcardAsyncPageGates;
    std::unordered_map<std::string, RouteGateRule> _routeGates;
    std::unordered_map<std::string, RouteGateRule> _wildcardRouteGates;
    std::unordered_map<std::string, AsyncRouteGateRule> _asyncRouteGates;
    std::unordered_map<std::string, AsyncRouteGateRule> _wildcardAsyncRouteGates;
};

}  // namespace geruest

#endif  // GERUEST_GATEREGISTRY_HPP
