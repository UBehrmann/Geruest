#include <gtest/gtest.h>

#include "data/HTTPRequest.hpp"
#include "data/ServerData.hpp"

using namespace geruest;

namespace {

HTTPRequest makeGetRequest(const std::string& path, const std::string& query = "") {
    std::string raw = "GET " + path;
    if (!query.empty()) {
        raw += "?" + query;
    }
    raw += " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    return HTTPRequest(std::move(raw), "127.0.0.1", "/tmp");
}

}  // namespace

TEST(ServerDataPageGate, ExactPathMatch) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/admin", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.findMatchingPageGate("/admin").has_value());
    EXPECT_FALSE(sd.findMatchingPageGate("/admin/extra").has_value());
    EXPECT_FALSE(sd.findMatchingPageGate("/other").has_value());
}

TEST(ServerDataPageGate, WildcardPathMatch) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/devices/*", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.findMatchingPageGate("/devices/foo").has_value());
    EXPECT_TRUE(sd.findMatchingPageGate("/devices/bar/baz").has_value());
    EXPECT_FALSE(sd.findMatchingPageGate("/devices").has_value());
}

TEST(ServerDataPageGate, RemoveAndClear) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/admin", [](const HTTPRequest&) { return true; }));
    ASSERT_TRUE(sd.addPageGate("/devices/*", [](const HTTPRequest&) { return true; }));

    EXPECT_TRUE(sd.removePageGate("/admin"));
    EXPECT_FALSE(sd.findMatchingPageGate("/admin").has_value());
    EXPECT_TRUE(sd.findMatchingPageGate("/devices/foo").has_value());

    sd.clearPageGates();
    EXPECT_FALSE(sd.findMatchingPageGate("/devices/foo").has_value());
}

TEST(ServerDataPageGate, CustomRedirectStored) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/login", [](const HTTPRequest&) { return false; }, "/signin"));

    auto gate = sd.findMatchingPageGate("/login");
    ASSERT_TRUE(gate.has_value());
    EXPECT_EQ(gate->redirectTo, "/signin");
}

TEST(ServerDataPageGate, HandlerGrantsOrDeniesAccess) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/token-page", [](const HTTPRequest& req) {
        return req.getParam("token") == "secret";
    }));

    auto gate = sd.findMatchingPageGate("/token-page");
    ASSERT_TRUE(gate.has_value());

    HTTPRequest allowed = makeGetRequest("/token-page", "token=secret");
    HTTPRequest denied = makeGetRequest("/token-page", "token=wrong");

    EXPECT_TRUE(gate->handler(allowed));
    EXPECT_FALSE(gate->handler(denied));
}

TEST(ServerDataPageGate, EmptyPathOrHandlerIgnored) {
    ServerData sd;
    EXPECT_FALSE(sd.addPageGate("", [](const HTTPRequest&) { return true; }));
    EXPECT_FALSE(sd.addPageGate("/valid", PageGateHandler{}));

    EXPECT_FALSE(sd.findMatchingPageGate("/valid").has_value());
}

TEST(ServerDataPageGate, DefaultRedirectHelper) {
    EXPECT_EQ(defaultPageGateRedirect("/de/secret"), "/de/");
    EXPECT_EQ(defaultPageGateRedirect("/fr/admin"), "/fr/");
    EXPECT_EQ(defaultPageGateRedirect("/secret"), "/");
    EXPECT_EQ(defaultPageGateRedirect("/"), "/");
}

TEST(ServerDataPageGate, ResolveRedirectPreservesRequestLanguage) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});

    EXPECT_EQ(sd.resolvePageGateRedirect("", "/de/secret"), "/de/");
    EXPECT_EQ(sd.resolvePageGateRedirect("/login", "/de/admin"), "/de/login");
    EXPECT_EQ(sd.resolvePageGateRedirect("/login", "/admin"), "/login");
    EXPECT_EQ(sd.resolvePageGateRedirect("/en/login", "/de/admin"), "/en/login");
    EXPECT_EQ(sd.resolvePageGateRedirect("https://example.com/login", "/de/admin"),
              "https://example.com/login");
}

TEST(ServerDataPageGate, CopyPreservesGates) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/admin", [](const HTTPRequest&) { return true; }, "/"));

    ServerData copy(sd);
    EXPECT_TRUE(copy.findMatchingPageGate("/admin").has_value());
}

TEST(ServerDataPageGate, LanguagePrefixMatchesBasePathGate) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});
    ASSERT_TRUE(sd.addPageGate("/admin", [](const HTTPRequest&) { return true; }));

    auto gate = sd.findMatchingPageGate("/de/admin");
    ASSERT_TRUE(gate.has_value());
}

TEST(ServerDataPageGate, WildcardPrefersLongestPattern) {
    ServerData sd;
    ASSERT_TRUE(sd.addPageGate("/admin/*", [](const HTTPRequest&) { return false; }, "/fallback"));
    ASSERT_TRUE(sd.addPageGate("/admin/secure/*", [](const HTTPRequest&) { return false; }, "/secure-login"));

    auto gate = sd.findMatchingPageGate("/admin/secure/panel");
    ASSERT_TRUE(gate.has_value());
    EXPECT_EQ(gate->redirectTo, "/secure-login");
}
