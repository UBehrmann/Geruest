/**
 * @file RouteDispatcher.cpp
 */

#include "RouteDispatcher.hpp"

#include "GzipResponse.hpp"
#include "Handler.hpp"
#include "StaticFileResolver.hpp"
#include "data/CorsConfig.hpp"
#include "data/MethodNotAllowed.hpp"

#include <exception>
#include <optional>

namespace geruest {

namespace {

boost::asio::awaitable<HTTPResponse> awaitReadyResponse(HTTPResponse response) {
    co_return std::move(response);
}

boost::asio::awaitable<HTTPResponse> rethrowAsResponse(std::exception_ptr error) {
    std::rethrow_exception(error);
    co_return HTTPResponse("500 Internal Server Error");
}

}  // namespace

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

        applyCorsHeaders(response, serverData_.getCorsConfig(), request);
        applyResponseCompression(response, request);
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send route response for: ";
        if (handlerKind == "async route") {
            failLog = "Failed to send async route response for: ";
        }
    } catch (const method_not_allowed& e) {
        HTTPResponse response = responseMethodNotAllowed(request, e.allowMethods());
        applyCorsHeaders(response, serverData_.getCorsConfig(), request);
        applyResponseCompression(response, request);
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send 405 for: ";
    } catch (const std::exception& e) {
        host.sendToLoggerError("Exception in " + handlerKind + " handler: " + e.what());
        HTTPResponse response = responseInternalServerError(request);
        applyCorsHeaders(response, serverData_.getCorsConfig(), request);
        applyResponseCompression(response, request);
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send 500 for: ";
    } catch (...) {
        host.sendToLoggerError("Unknown exception in " + handlerKind + " handler");
        HTTPResponse response = responseInternalServerError(request);
        applyCorsHeaders(response, serverData_.getCorsConfig(), request);
        applyResponseCompression(response, request);
        response.serializeTo(host.responseScratch_);
        failLog = "Failed to send 500 for: ";
    }

    if (failLog != nullptr && !co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
        host.sendToLoggerError(std::string(failLog) + request->getPathString());
    }
    co_return;
}

boost::asio::awaitable<bool> RouteDispatcher::sendRouteGateDenialAsync(HTTPRequest* request, const std::string& path,
                                                                       Handler& host) {
    auto denial = co_await host.checkRouteGateDenialAsync(*request);
    if (!denial) {
        co_return false;
    }
    applyCorsHeaders(*denial, serverData_.getCorsConfig(), request);
    applyResponseCompression(*denial, request);
    denial->serializeTo(host.responseScratch_);
    if (!co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
        host.sendToLoggerError("Failed to send route gate denial for: " + path);
    }
    co_return true;
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

        applyCorsHeaders(redirectResponse, serverData_.getCorsConfig(), request);
        redirectResponse.serializeTo(host.responseScratch_);
        if (!co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
            host.sendToLoggerError("Failed to send redirect response for: " + request->getPathString());
        }
        co_return;
    }

    std::string path = request->getPathString();

    // Async wins when sync and async handlers share the same path pattern.
    if (auto asyncRouteHandler = serverData_.findMatchingAsyncRoute(path)) {
        if (co_await sendRouteGateDenialAsync(request, path, host)) {
            co_return;
        }
        co_await dispatchRouteAndSendAsync(
            request, path,
            [asyncRouteHandler, request]() -> boost::asio::awaitable<HTTPResponse> {
                co_return co_await (*asyncRouteHandler)(*request);
            }(),
            "async route", host);
        co_return;
    }

    if (auto routeHandler = serverData_.findMatchingRoute(path)) {
        if (co_await sendRouteGateDenialAsync(request, path, host)) {
            co_return;
        }
        // ponytail: run sync handlers on the dispatcher coroutine stack (not inside a nested
        // coroutine frame). /status builds a large JSON tree; nesting caused stack overflow → 139.
        std::optional<HTTPResponse> syncResponse;
        std::exception_ptr syncError;
        try {
            syncResponse.emplace((*routeHandler)(*request));
        } catch (...) {
            syncError = std::current_exception();
        }
        if (syncError) {
            co_await dispatchRouteAndSendAsync(request, path, rethrowAsResponse(syncError), "route", host);
        } else {
            co_await dispatchRouteAndSendAsync(request, path, awaitReadyResponse(std::move(*syncResponse)), "route",
                                               host);
        }
        co_return;
    }

    std::string extension = StaticFileResolver::getExtension(path);
    const std::string contentType = StaticFileResolver::getContentType(extension);
    const std::string contentPath = fileResolver_.buildPath(path, extension, *request);

    co_await host.sendFileAsync(contentType, contentPath, request);
    co_return;
}

}  // namespace geruest
