/**
 * @file RouteRegistry.hpp
 * @brief HTTP/WebSocket routes, redirects, and lookup.
 */

#ifndef GERUEST_ROUTEREGISTRY_HPP
#define GERUEST_ROUTEREGISTRY_HPP

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "LanguageConfig.hpp"
#include "ServerTypes.hpp"
#include "server/WebSocketTypes.hpp"

namespace geruest {

class RouteRegistry {
   public:
    RouteRegistry() = default;
    explicit RouteRegistry(const LanguageConfig* languages) : _languages(languages) {}
    RouteRegistry(const RouteRegistry&) = default;
    RouteRegistry& operator=(const RouteRegistry&) = default;

    void setLanguageConfig(const LanguageConfig* languages) { _languages = languages; }

    std::unordered_map<std::string, RouteHandler> getRoutesMerged() const;
    const std::unordered_map<std::string, RouteHandler>& getExactRoutes() const { return _routes; }

    void addRoute(const std::string& path, RouteHandler routeHandler);
    void addRouteAsync(const std::string& path, AsyncRouteHandler routeHandler);
    void addWebSocketRoute(const std::string& path, WebSocketHandler routeHandler);

    bool addRedirect(const std::string& from, const std::string& to, int status = 301);
    size_t addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status = 301);

    std::optional<std::pair<std::string, int>> findMatchingRedirect(const std::string& path) const;

    std::optional<RouteHandler> findMatchingRoute(const std::string& path) const;
    std::optional<AsyncRouteHandler> findMatchingAsyncRoute(const std::string& path) const;
    std::optional<WebSocketHandler> findMatchingWebSocketRoute(const std::string& path) const;

    void setWebSocketMaxMessageBytes(size_t bytes) { _webSocketLimits.maxMessageBytes = bytes; }
    void setWebSocketMaxFrameBytes(size_t bytes) { _webSocketLimits.maxFrameBytes = bytes; }
    void setWebSocketIdleTimeout(std::chrono::seconds seconds) { _webSocketLimits.idleTimeout = seconds; }
    void setWebSocketPingInterval(std::chrono::seconds seconds) { _webSocketLimits.pingInterval = seconds; }
    void addWebSocketSubprotocol(std::string name) { _webSocketSubprotocols.push_back(std::move(name)); }

    const WebSocketLimits& getWebSocketLimits() const { return _webSocketLimits; }
    const std::vector<std::string>& getWebSocketSubprotocols() const { return _webSocketSubprotocols; }

   private:
    struct RedirectRule {
        std::string target;
        int status = 301;
    };

    template <typename Rule>
    std::optional<Rule> findMatchingRouteImpl(const std::unordered_map<std::string, Rule>& exactRoutes,
                                              const std::unordered_map<std::string, Rule>& wildcardRoutes,
                                              const std::string& path) const;

    std::optional<std::pair<std::string, int>> findMatchingWildcardRedirect(const std::string& path,
                                                                            const std::string& requestPath) const;
    bool hasRedirectLoop(const std::string& startPath) const;

    const LanguageConfig* _languages = nullptr;

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
};

}  // namespace geruest

#endif  // GERUEST_ROUTEREGISTRY_HPP
