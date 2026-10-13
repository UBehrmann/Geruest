/**
 * @file RouteDispatcher.hpp
 * @brief HTTP route dispatch: redirect, sync/async routes, static files.
 */

#ifndef GERUEST_ROUTEDISPATCHER_HPP
#define GERUEST_ROUTEDISPATCHER_HPP

#include <boost/asio/awaitable.hpp>
#include <string>
#include <string_view>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"

namespace geruest {

class Handler;
class StaticFileResolver;

class RouteDispatcher {
   public:
    RouteDispatcher(const ServerData& serverData, StaticFileResolver& fileResolver);

    boost::asio::awaitable<void> dispatchAsync(HTTPRequest* request, Handler& host);

   private:
    boost::asio::awaitable<void> dispatchRouteAndSendAsync(HTTPRequest* request, const std::string& path,
                                                           boost::asio::awaitable<HTTPResponse> produced,
                                                           std::string_view handlerLabel, Handler& host);

    /** Sends the route gate's denial if it refuses the request; true when the request was answered. */
    boost::asio::awaitable<bool> sendRouteGateDenialAsync(HTTPRequest* request, const std::string& path,
                                                          Handler& host);

    const ServerData& serverData_;
    StaticFileResolver& fileResolver_;
};

}  // namespace geruest

#endif  // GERUEST_ROUTEDISPATCHER_HPP
