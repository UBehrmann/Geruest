#include <gtest/gtest.h>

#include "data/ServerData.hpp"

using namespace geruest;

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
