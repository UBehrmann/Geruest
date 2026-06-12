/**
 * @file WebSocketServerData_tests.cpp
 * @brief Unit tests for WebSocket route registration in ServerData.
 */

#include <gtest/gtest.h>

#include <atomic>

#include "../../data/HTTPRequest.hpp"
#include "../../data/ServerData.hpp"
#include "../../server/WebSocket.hpp"

using namespace geruest;

TEST(WebSocketServerData, AddAndFindExact) {
    ServerData data;
    bool called = false;
    data.addWebSocketRoute(
        "/ws/chat",
        [&called](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> {
            called = true;
            co_return;
        });
    const auto handler = data.findMatchingWebSocketRoute("/ws/chat");
    ASSERT_TRUE(handler.has_value());
    EXPECT_FALSE(data.findMatchingWebSocketRoute("/ws/other").has_value());
    (void)called;
}

TEST(WebSocketServerData, WildcardMatch) {
    ServerData data;
    data.addWebSocketRoute(
        "/ws/*",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    EXPECT_TRUE(data.findMatchingWebSocketRoute("/ws/anything").has_value());
}

TEST(WebSocketServerData, ExactBeatsWildcard) {
    ServerData data;
    data.addWebSocketRoute(
        "/ws/chat",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    data.addWebSocketRoute(
        "/ws/*",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    const auto handler = data.findMatchingWebSocketRoute("/ws/chat");
    ASSERT_TRUE(handler.has_value());
}

TEST(WebSocketServerData, NoCrossContaminationWithHttpRoutes) {
    ServerData data;
    data.addWebSocketRoute(
        "/api/ws",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    EXPECT_FALSE(data.findMatchingRoute("/api/ws").has_value());
    EXPECT_FALSE(data.findMatchingAsyncRoute("/api/ws").has_value());
}

TEST(WebSocketServerData, CopyPreservesRoutes) {
    ServerData original;
    original.addWebSocketRoute(
        "/echo",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    const ServerData copy = original;
    EXPECT_TRUE(copy.findMatchingWebSocketRoute("/echo").has_value());
}

TEST(WebSocketServerData, LimitsAndSubprotocols) {
    ServerData data;
    EXPECT_EQ(data.getWebSocketLimits().maxMessageBytes, 16U * 1024 * 1024);
    data.setWebSocketMaxFrameBytes(1024);
    EXPECT_EQ(data.getWebSocketLimits().maxFrameBytes, 1024U);
    data.addWebSocketSubprotocol("chat");
    data.addWebSocketSubprotocol("v2");
    ASSERT_EQ(data.getWebSocketSubprotocols().size(), 2U);
    EXPECT_EQ(data.getWebSocketSubprotocols()[0], "chat");
}
