/**
 * @file ServerTypes.hpp
 * @brief Shared route/gate handler types and path normalization helpers.
 */

#ifndef GERUEST_SERVERTYPES_HPP
#define GERUEST_SERVERTYPES_HPP

#include <boost/asio/awaitable.hpp>
#include <cctype>
#include <functional>
#include <string>

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

namespace geruest {

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
using AsyncRouteGateAccess = boost::asio::awaitable<bool>;
using AsyncRouteGateHandler = std::function<AsyncRouteGateAccess(const HTTPRequest&)>;

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

struct AsyncRouteGateRule {
    AsyncRouteGateHandler handler;
};

/** Winning route gate after sync/async and wildcard resolution. */
struct ResolvedRouteGate {
    bool async = false;
    RouteGateHandler syncHandler;
    AsyncRouteGateHandler asyncHandler;
};

/**
 * Normalize request path for gate/route lookup.
 * Strips trailing slashes (except root) and .html/.htm so gates match extensionless URLs.
 */
inline std::string canonicalRequestPath(std::string path) {
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".html") == 0) {
        path.resize(path.size() - 5);
    } else if (path.size() > 4 && path.compare(path.size() - 4, 4, ".htm") == 0) {
        path.resize(path.size() - 4);
    }
    return path;
}

/** Default redirect when a page gate denies access and no custom target is set. */
inline std::string defaultPageGateRedirect(const std::string& requestPath) {
    if (requestPath.size() >= 4 && requestPath[0] == '/' &&
        std::isalpha(static_cast<unsigned char>(requestPath[1])) &&
        std::isalpha(static_cast<unsigned char>(requestPath[2])) && requestPath[3] == '/') {
        return requestPath.substr(0, 4);
    }
    return "/";
}

}  // namespace geruest

#endif  // GERUEST_SERVERTYPES_HPP
