/**
 * @file Handler.hpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief HTTP connection handler using Boost.Asio async I/O (C++20 coroutines).
 */

#ifndef GERUEST_HANDLER_HPP
#define GERUEST_HANDLER_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "HttpFraming.hpp"
#include "ResponseWriter.hpp"
#include "RouteDispatcher.hpp"
#include "StaticFileResolver.hpp"
#include "data/HTTPRequest.hpp"
#include "data/ServerData.hpp"

namespace geruest {

enum class PageAccessDenyStyle { Redirect, Forbidden };

class Handler;
class HTTPRequest;

namespace websocket {
boost::asio::awaitable<bool> handleUpgrade(Handler& host, HTTPRequest* request);
void ensureWebSocketModuleRegistered();
}

class Handler {
    friend class RouteDispatcher;
    friend class ResponseWriter;
    friend boost::asio::awaitable<bool> websocket::handleUpgrade(Handler&, HTTPRequest*);

   private:
    boost::asio::ip::tcp::socket& clientSocket;

    std::istringstream requestStream;

    const ServerData& serverData;

    const std::string IP;

    bool _upgraded = false;

    /** False for /status and other monitoring paths excluded from metrics. */
    bool _countRequestInMetrics = true;

    ResponseWriter writer_;
    /** Alias for RouteDispatcher; owned by writer_. */
    std::string& responseScratch_;

    HttpFraming framing_;

    StaticFileResolver fileResolver_;
    RouteDispatcher    routeDispatcher_;

    void record4xxMetric() const;
    void record5xxMetric() const;
    void recordErrorMetric() const;

    boost::asio::awaitable<bool> enforcePageAccessAsync(const HTTPRequest& request, const std::string& pagePath,
                                                        PageAccessDenyStyle denyStyle,
                                                        const std::optional<ResolvedPageGate>& resolvedGate =
                                                            std::nullopt);

    boost::asio::awaitable<std::optional<HTTPResponse>> checkRouteGateDenialAsync(
        const HTTPRequest& request) const;

    boost::asio::awaitable<bool> sendSocketAsync(const char* bufferToSend, size_t size);

    void sendToLogger(const std::string& message, LogLevel level = LogLevel::Info) const;

    void sendToLoggerPages(const std::string& message) const;

    void sendToLoggerAPI(const std::string& message) const;

    void sendToLoggerUser(const std::string& message) const;

    void sendToLoggerError(const std::string& message) const;

    boost::asio::awaitable<void> handleRequestAsync(HTTPRequest* request);

    boost::asio::awaitable<bool> tryHandleWebSocketAsync(HTTPRequest* request);

    void markUpgraded() { _upgraded = true; }

    boost::asio::awaitable<void> sendFileAsync(const std::string& contentType, const std::string& contentPath,
                                               HTTPRequest* httpRequest);

    boost::asio::awaitable<void> sendResponseAsync(const std::string& status, const std::string& contentType,
                                                   const std::string& content);

   public:
    Handler(boost::asio::ip::tcp::socket& socket, std::string clientIP, const ServerData& serverDataRef);

    ~Handler();

    boost::asio::awaitable<void> runAsync();
};

}  // namespace geruest

#endif  // GERUEST_HANDLER_HPP
