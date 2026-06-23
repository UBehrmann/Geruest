#include "Geruest.hpp"

#include "geruest/BuildConfig.hpp"
#include "server/WebSocket.hpp"

#if GERUEST_ENABLE_WEBSOCKET

namespace geruest {

void Geruest::addRouteWebSocket(const std::string& path, WebSocketHandler handler) {
    serverData.addWebSocketRoute(path, std::move(handler));
    sendToLogger("Added WebSocket route: " + path);
}

void Geruest::addRouteWebSocket(const std::string& path, WebSocketRoute route) {
    serverData.addWebSocketRoute(path, adaptWebSocketRoute(std::move(route)));
    sendToLogger("Added WebSocket callback route: " + path);
}

void Geruest::addRouteWebSocket(const std::string& path, WebSocketHandler handler, RouteGateHandler gate) {
    if (path.empty() || !handler || !gate) {
        sendToLoggerError("Failed to add WebSocket route with gate (path/handler/gate invalid): " + path);
        return;
    }
    serverData.addWebSocketRoute(path, std::move(handler));
    serverData.addRouteGate(path, std::move(gate));
    sendToLogger("Added WebSocket route with gate: " + path);
}

void Geruest::addRouteWebSocket(const std::string& path, WebSocketHandler handler, AsyncRouteGateHandler gate) {
    if (path.empty() || !handler || !gate) {
        sendToLoggerError("Failed to add WebSocket route with async gate (path/handler/gate invalid): " + path);
        return;
    }
    serverData.addWebSocketRoute(path, std::move(handler));
    serverData.addAsyncRouteGate(path, std::move(gate));
    sendToLogger("Added WebSocket route with async gate: " + path);
}

void Geruest::addRouteWebSocket(const std::string& path, WebSocketRoute route, RouteGateHandler gate) {
    if (path.empty() || !gate) {
        sendToLoggerError("Failed to add WebSocket callback route with gate (path/gate invalid): " + path);
        return;
    }
    serverData.addWebSocketRoute(path, adaptWebSocketRoute(std::move(route)));
    serverData.addRouteGate(path, std::move(gate));
    sendToLogger("Added WebSocket callback route with gate: " + path);
}

void Geruest::addRouteWebSocket(const std::string& path, WebSocketRoute route, AsyncRouteGateHandler gate) {
    if (path.empty() || !gate) {
        sendToLoggerError("Failed to add WebSocket callback route with async gate (path/gate invalid): " + path);
        return;
    }
    serverData.addWebSocketRoute(path, adaptWebSocketRoute(std::move(route)));
    serverData.addAsyncRouteGate(path, std::move(gate));
    sendToLogger("Added WebSocket callback route with async gate: " + path);
}

void Geruest::setWebSocketMaxMessageBytes(size_t bytes) {
    serverData.setWebSocketMaxMessageBytes(bytes);
    sendToLogger("WebSocket max message bytes set to: " + std::to_string(bytes));
}

void Geruest::setWebSocketMaxFrameBytes(size_t bytes) {
    serverData.setWebSocketMaxFrameBytes(bytes);
    sendToLogger("WebSocket max frame bytes set to: " + std::to_string(bytes));
}

void Geruest::setWebSocketIdleTimeout(int seconds) {
    serverData.setWebSocketIdleTimeout(std::chrono::seconds(seconds));
    sendToLogger("WebSocket idle timeout set to: " + std::to_string(seconds) + "s");
}

void Geruest::setWebSocketPingInterval(int seconds) {
    serverData.setWebSocketPingInterval(std::chrono::seconds(seconds));
    sendToLogger("WebSocket ping interval set to: " + std::to_string(seconds) + "s");
}

void Geruest::addWebSocketSubprotocol(const std::string& name) {
    serverData.addWebSocketSubprotocol(name);
    sendToLogger("Added WebSocket subprotocol: " + name);
}

}  // namespace geruest

#endif  // GERUEST_ENABLE_WEBSOCKET
