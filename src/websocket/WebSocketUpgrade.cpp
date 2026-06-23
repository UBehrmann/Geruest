#include "websocket/WebSocketUpgrade.hpp"

#include "handler/Handler.hpp"
#include "handler/GateEvaluation.hpp"
#include "modules/ModuleHooks.hpp"
#include "server/WebSocket.hpp"
#include "data/CorsConfig.hpp"
#include "data/HTTPResponse.hpp"

namespace geruest::websocket {

boost::asio::awaitable<bool> handleUpgrade(Handler& host, HTTPRequest* request) {
    if (request == nullptr) {
        co_return false;
    }

    if (request->getMethod() != "GET") {
        co_return false;
    }

    if (!isWebSocketUpgrade(*request)) {
        if (isWebSocketUpgradeIntent(*request) && host.serverData.findMatchingWebSocketRoute(request->getPathString())) {
            HTTPResponse br = responseBadRequest(request);
            br.serializeTo(host.responseScratch_);
            co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size());
            host.markUpgraded();
            co_return true;
        }
        co_return false;
    }

    if (!request->getBody().empty()) {
        HTTPResponse br = responseBadRequest(request);
        br.serializeTo(host.responseScratch_);
        co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size());
        host.markUpgraded();
        co_return true;
    }

    auto wsHandler = host.serverData.findMatchingWebSocketRoute(request->getPathString());
    if (!wsHandler) {
        co_await host.writer_.sendNotFoundResponseAsync(request, host);
        host.markUpgraded();
        co_return true;
    }

    if (auto denial = co_await host.checkRouteGateDenialAsync(*request)) {
        host.record4xxMetric();
        applyCorsHeaders(*denial, host.serverData.getCorsConfig(), request);
        denial->serializeTo(host.responseScratch_);
        if (!co_await host.sendSocketAsync(host.responseScratch_.data(), host.responseScratch_.size())) {
            host.sendToLoggerError("Failed to send WebSocket gate denial for: " + request->getPathString());
        }
        host.markUpgraded();
        co_return true;
    }

    const std::string secKey = request->getHeader("sec-websocket-key");
    const std::string acceptKey = computeAcceptKey(secKey);
    const std::string subprotocol =
        pickSubprotocol(request->getHeaderView("sec-websocket-protocol"), host.serverData.getWebSocketSubprotocols());
    const std::string handshake = buildHandshakeResponse(acceptKey, subprotocol);
    if (!co_await host.sendSocketAsync(handshake.c_str(), handshake.size())) {
        host.markUpgraded();
        co_return true;
    }

    WebSocketConnection ws(host.clientSocket, host.IP, subprotocol, host.serverData.getWebSocketLimits());
    bool handlerFailed = false;
    try {
        co_await (*wsHandler)(ws, *request);
    } catch (const std::exception& e) {
        handlerFailed = true;
        host.record5xxMetric();
        host.sendToLoggerError(std::string("Exception in WebSocket handler: ") + e.what());
    } catch (...) {
        handlerFailed = true;
        host.record5xxMetric();
        host.sendToLoggerError("Unknown exception in WebSocket handler");
    }

    if (ws.isOpen()) {
        if (handlerFailed) {
            co_await ws.close(1011, "internal error");
        } else {
            co_await ws.close(1000, "");
        }
    }

    host.markUpgraded();
    co_return true;
}

namespace {

struct WebSocketModuleRegistrar {
    WebSocketModuleRegistrar() { modules::registerWebSocketUpgrade(handleUpgrade); }
};

}  // namespace

void ensureWebSocketModuleRegistered() {
    static const WebSocketModuleRegistrar instance;
    (void)instance;
}

}  // namespace geruest::websocket
