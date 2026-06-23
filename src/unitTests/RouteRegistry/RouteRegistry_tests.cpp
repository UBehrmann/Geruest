#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/RouteRegistry.hpp"

using namespace geruest;

namespace {

HTTPRequest makeGetRequest(const std::string& path) {
    const std::string raw = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    return HTTPRequest(std::move(raw), "127.0.0.1", "/tmp");
}

enum class DispatchKind { None, Sync, Async };

DispatchKind resolveRouteKind(const RouteRegistry& registry, const std::string& path) {
    // Mirrors RouteDispatcher::dispatchAsync lookup order.
    if (registry.findMatchingAsyncRoute(path).has_value()) {
        return DispatchKind::Async;
    }
    if (registry.findMatchingRoute(path).has_value()) {
        return DispatchKind::Sync;
    }
    return DispatchKind::None;
}

}  // namespace

TEST(RouteRegistry, AddRouteAcceptsAsyncHandler) {
    RouteRegistry registry;
    registry.addRoute("/api/db", [](const HTTPRequest&) -> AsyncResponse {
        co_return HTTPResponse("200 OK");
    });
    EXPECT_TRUE(registry.findMatchingAsyncRoute("/api/db").has_value());
    EXPECT_FALSE(registry.findMatchingRoute("/api/db").has_value());
}

TEST(RouteRegistry, AddRouteAsyncOverload) {
    RouteRegistry registry;
    registry.addRoute("/api/db", [](const HTTPRequest&) -> AsyncResponse {
        co_return HTTPResponse("200 OK");
    });
    EXPECT_TRUE(registry.findMatchingAsyncRoute("/api/db").has_value());
}

TEST(RouteRegistry, AsyncWinsWhenSyncAndAsyncSharePath) {
    RouteRegistry registry;
    registry.addRoute("/api/x", [](const HTTPRequest&) { return HTTPResponse("200 sync"); });
    registry.addRoute("/api/x", [](const HTTPRequest&) -> AsyncResponse {
        co_return HTTPResponse("200 async");
    });

    EXPECT_TRUE(registry.findMatchingRoute("/api/x").has_value());
    EXPECT_TRUE(registry.findMatchingAsyncRoute("/api/x").has_value());
    EXPECT_EQ(resolveRouteKind(registry, "/api/x"), DispatchKind::Async);
}

TEST(RouteRegistry, AsyncHandlerRunsWhenBothRegistered) {
    RouteRegistry registry;
    bool syncCalled = false;
    bool asyncCalled = false;

    registry.addRoute("/api/x", [&](const HTTPRequest&) {
        syncCalled = true;
        return HTTPResponse("200 sync");
    });
    registry.addRoute("/api/x", [&](const HTTPRequest&) -> AsyncResponse {
        asyncCalled = true;
        co_return HTTPResponse("200 async");
    });

    ASSERT_EQ(resolveRouteKind(registry, "/api/x"), DispatchKind::Async);

    boost::asio::io_context io;
    auto handler = registry.findMatchingAsyncRoute("/api/x");
    ASSERT_TRUE(handler.has_value());
    auto request = makeGetRequest("/api/x");
    auto done = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            (void)co_await (*handler)(request);
        },
        boost::asio::use_future);
    io.run();
    done.get();

    EXPECT_TRUE(asyncCalled);
    EXPECT_FALSE(syncCalled);
}
