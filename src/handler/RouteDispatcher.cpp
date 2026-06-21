/**
 * @file RouteDispatcher.cpp
 */

#include "RouteDispatcher.hpp"

#include "Handler.hpp"
#include "StaticFileResolver.hpp"
#include "data/MethodNotAllowed.hpp"

namespace geruest {

RouteDispatcher::RouteDispatcher(const ServerData& serverData, StaticFileResolver& fileResolver)
    : serverData_(serverData), fileResolver_(fileResolver) {}

boost::asio::awaitable<void> RouteDispatcher::dispatchRouteAndSendAsync(HTTPRequest* request,
                                                                        const std::string& /*path*/,
                                                                        boost::asio::awaitable<HTTPResponse> produced,
                                                                        std::string_view handlerLabel,
                                                                        Handler& host) {
    const std::string handlerKind(handlerLabel);
    const char* failLog = nullptr;
    try {
        HTTPResponse response = co_await std::move(produced);

        const std::string& status = response.getStatus();
        if (!status.empty()) {
            if (status[0] == '4') {
                host.record4xxMetric();
            } else if (status[0] == '5') {
                host.record5xxMetric();
            }
        }

        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send route response for: ";
        if (handlerKind == "async route") {
            failLog = "Failed to send async route response for: ";
        }
    } catch (const method_not_allowed& e) {
        HTTPResponse response = responseMethodNotAllowed(request, e.allowMethods());
        host.record4xxMetric();
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send 405 for: ";
    } catch (const std::exception& e) {
        host.sendToLoggerError("Exception in " + handlerKind + " handler: " + e.what());
        HTTPResponse response = responseInternalServerError(request);
        host.record5xxMetric();
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send 500 for: ";
    } catch (...) {
        host.sendToLoggerError("Unknown exception in " + handlerKind + " handler");
        HTTPResponse response = responseInternalServerError(request);
        host.record5xxMetric();
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send 500 for: ";
    }

    if (!co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
        host.sendToLoggerError(std::string(failLog) + request->getPathString());
    }
    co_return;
}

boost::asio::awaitable<void> RouteDispatcher::tryDispatchRoute(HTTPRequest* request, const std::string& path,
                                                               boost::asio::awaitable<HTTPResponse> produced,
                                                               std::string_view handlerLabel, Handler& host) {
    if (auto denial = co_await host.checkRouteGateDenialAsync(*request)) {
        host.record4xxMetric();
        denial->serializeTo(host.responseScratch_);
        if (!co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
            host.sendToLoggerError("Failed to send route gate denial for: " + path);
        }
        co_return;
    }

    co_await dispatchRouteAndSendAsync(request, path, std::move(produced), handlerLabel, host);
}

boost::asio::awaitable<void> RouteDispatcher::dispatchAsync(HTTPRequest* request, Handler& host) {
    auto redirectMatch = serverData_.findMatchingRedirect(request->getPathString());
    if (redirectMatch.has_value()) {
        const std::string& target = redirectMatch->first;
        const int statusCode = redirectMatch->second;
        const std::string statusText = (statusCode == 302) ? "302 Found" : "301 Moved Permanently";

        HTTPResponse redirectResponse(statusText);
        redirectResponse.setHeader("Location", target);
        redirectResponse.setBody("");

        redirectResponse.serializeTo(host.responseScratch_);
        if (!co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
            host.sendToLoggerError("Failed to send redirect response for: " + request->getPathString());
        }
        co_return;
    }

    std::string path = request->getPathString();

    if (auto routeHandler = serverData_.findMatchingRoute(path)) {
        co_await tryDispatchRoute(
            request, path,
            [routeHandler, request]() -> boost::asio::awaitable<HTTPResponse> {
                co_return (*routeHandler)(*request);
            }(),
            "route", host);
        co_return;
    }

    if (auto asyncRouteHandler = serverData_.findMatchingAsyncRoute(path)) {
        co_await tryDispatchRoute(
            request, path,
            [asyncRouteHandler, request]() -> boost::asio::awaitable<HTTPResponse> {
                co_return co_await (*asyncRouteHandler)(*request);
            }(),
            "async route", host);
        co_return;
    }

    if (path.rfind("/api/", 0) == 0) {
        host.sendToLoggerError("No API route matched. path=" + path + " request_line=" + request->getRawRequestLine());
    }

    std::string extension = StaticFileResolver::getExtension(path);
    const std::string contentType = StaticFileResolver::getContentType(extension);
    const std::string contentPath = fileResolver_.buildPath(path, extension, *request);

    co_await host.sendFileAsync(contentType, contentPath, request);
    co_return;
}

}  // namespace geruest
