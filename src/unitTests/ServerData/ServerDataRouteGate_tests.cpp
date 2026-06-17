#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"

using namespace geruest;

namespace {

HTTPRequest makeGetRequest(const std::string& path) {
    const std::string raw = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    return HTTPRequest(std::move(raw), "127.0.0.1", "/tmp");
}

}  // namespace

TEST(ServerDataRouteGate, ExactPathMatch) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/admin", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.findMatchingRouteGate("/v1/admin").has_value());
    EXPECT_FALSE(sd.findMatchingRouteGate("/v1/admin/extra").has_value());
    EXPECT_FALSE(sd.findMatchingRouteGate("/other").has_value());
}

TEST(ServerDataRouteGate, WildcardPathMatch) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/*", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.findMatchingRouteGate("/v1/foo").has_value());
    EXPECT_TRUE(sd.findMatchingRouteGate("/v1/bar/baz").has_value());
    EXPECT_FALSE(sd.findMatchingRouteGate("/v1").has_value());
}

TEST(ServerDataRouteGate, RemoveAndClear) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/admin", [](const HTTPRequest&) { return true; }));
    ASSERT_TRUE(sd.addRouteGate("/v1/public/*", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.removeRouteGate("/v1/admin"));
    EXPECT_FALSE(sd.findMatchingRouteGate("/v1/admin").has_value());
    EXPECT_TRUE(sd.findMatchingRouteGate("/v1/public/foo").has_value());

    sd.clearRouteGates();
    EXPECT_FALSE(sd.findMatchingRouteGate("/v1/public/foo").has_value());
}

TEST(ServerDataRouteGate, EmptyPathOrHandlerIgnored) {
    ServerData sd;
    EXPECT_FALSE(sd.addRouteGate("", [](const HTTPRequest&) { return true; }));
    EXPECT_FALSE(sd.addRouteGate("/valid", RouteGateHandler{}));

    EXPECT_FALSE(sd.findMatchingRouteGate("/valid").has_value());
}

TEST(ServerDataRouteGate, CopyPreservesGates) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/admin", [](const HTTPRequest&) { return true; }));

    ServerData copy(sd);
    EXPECT_TRUE(copy.findMatchingRouteGate("/v1/admin").has_value());
}

TEST(ServerDataRouteGate, WildcardPrefersLongestPattern) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/*", [](const HTTPRequest&) { return false; }));
    ASSERT_TRUE(sd.addRouteGate("/v1/secure/*", [](const HTTPRequest&) { return true; }));

    auto gate = sd.findMatchingRouteGate("/v1/secure/panel");
    ASSERT_TRUE(gate.has_value());
}

TEST(ServerDataRouteGate, PageAndRouteGatesAreIndependent) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/admin", [](const HTTPRequest&) { return true; }));
    ASSERT_TRUE(sd.addRouteGate("/v1/admin", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.findMatchingPageGate("/admin").has_value());
    EXPECT_FALSE(sd.findMatchingRouteGate("/admin").has_value());
    EXPECT_TRUE(sd.findMatchingRouteGate("/v1/admin").has_value());
    EXPECT_FALSE(sd.findMatchingPageGate("/v1/admin").has_value());
}

TEST(ServerDataRouteGate, CanonicalPathStripsTrailingSlash) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/admin", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.findMatchingRouteGate("/v1/admin/").has_value());
    EXPECT_TRUE(sd.findResolvedRouteGate("/v1/admin/").has_value());
}

TEST(ServerDataRouteGate, OverlappingWildcardsRouteMatchesLongest) {
    ServerData sd;
    std::string matched;

    sd.addRoute("/v1/*", [&](const HTTPRequest&) {
        matched = "wide";
        return HTTPResponse("200 OK");
    });
    sd.addRoute("/v1/secure/*", [&](const HTTPRequest&) {
        matched = "secure";
        return HTTPResponse("200 OK");
    });

    auto handler = sd.findMatchingRoute("/v1/secure/panel");
    ASSERT_TRUE(handler.has_value());
    (*handler)(makeGetRequest("/v1/secure/panel"));
    EXPECT_EQ(matched, "secure");
}

TEST(ServerDataRouteGate, OverlappingWildcardsGateAndRouteAgree) {
    ServerData sd;
    bool wideGateAllowed = true;
    bool secureGateAllowed = false;

    sd.addRoute("/v1/*", [](const HTTPRequest&) { return HTTPResponse("200 OK"); });
    sd.addRoute("/v1/secure/*", [](const HTTPRequest&) { return HTTPResponse("200 OK"); });
    ASSERT_TRUE(sd.addRouteGate("/v1/*", [&](const HTTPRequest&) { return wideGateAllowed; }));
    ASSERT_TRUE(sd.addRouteGate("/v1/secure/*", [&](const HTTPRequest&) { return secureGateAllowed; }));

    auto route = sd.findMatchingRoute("/v1/secure/panel");
    auto gate = sd.findResolvedRouteGate("/v1/secure/panel");
    ASSERT_TRUE(route.has_value());
    ASSERT_TRUE(gate.has_value());
    EXPECT_FALSE(gate->async);
    EXPECT_FALSE(gate->syncHandler(makeGetRequest("/v1/secure/panel")));
}

TEST(ServerDataRouteGate, AsyncRouteGateWinsOnSamePath) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/admin", [](const HTTPRequest&) { return true; }));
    ASSERT_TRUE(sd.addAsyncRouteGate("/v1/admin", [](const HTTPRequest&) -> AsyncRouteGateAccess {
        co_return false;
    }));

    auto gate = sd.findResolvedRouteGate("/v1/admin");
    ASSERT_TRUE(gate.has_value());
    EXPECT_TRUE(gate->async);

    boost::asio::io_context io;
    auto future = boost::asio::co_spawn(io, gate->asyncHandler(makeGetRequest("/v1/admin")),
                                        boost::asio::use_future);
    io.run();
    EXPECT_FALSE(future.get());
}

TEST(ServerDataRouteGate, ResolvedPrefersLongestWildcardAcrossSyncAndAsync) {
    ServerData sd;
    ASSERT_TRUE(sd.addRouteGate("/v1/*", [](const HTTPRequest&) { return false; }));
    ASSERT_TRUE(sd.addAsyncRouteGate("/v1/secure/*", [](const HTTPRequest&) -> AsyncRouteGateAccess {
        co_return true;
    }));

    auto gate = sd.findResolvedRouteGate("/v1/secure/panel");
    ASSERT_TRUE(gate.has_value());
    EXPECT_TRUE(gate->async);
}
