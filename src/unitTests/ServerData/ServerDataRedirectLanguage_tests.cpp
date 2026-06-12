#include <gtest/gtest.h>

#include "data/ServerData.hpp"

using namespace geruest;

TEST(ServerDataRedirectLanguage, ExactRedirectPreservesRequestLanguagePrefix) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});

    ASSERT_TRUE(sd.addRedirect("/something", "/redirection"));
    const auto match = sd.findMatchingRedirect("/de/something");

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->first, "/de/redirection");
    EXPECT_EQ(match->second, 301);
}

TEST(ServerDataRedirectLanguage, WildcardRedirectPreservesRequestLanguagePrefix) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});

    ASSERT_TRUE(sd.addRedirect("*/redirect", "/redirection"));
    const auto match = sd.findMatchingRedirect("/de/something/redirect");

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->first, "/de/redirection");
    EXPECT_EQ(match->second, 301);
}

TEST(ServerDataRedirectLanguage, ExplicitLanguageTargetStaysUnchanged) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});

    ASSERT_TRUE(sd.addRedirect("*/redirect", "/en/redirection"));
    const auto match = sd.findMatchingRedirect("/de/something/redirect");

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->first, "/en/redirection");
}

TEST(ServerDataRedirectLanguage, NoLanguageInRequestKeepsExistingBehavior) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});

    ASSERT_TRUE(sd.addRedirect("/something", "/redirection"));
    const auto match = sd.findMatchingRedirect("/something");

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->first, "/redirection");
}

TEST(ServerDataRedirectLanguage, ExternalTargetNeverChanged) {
    ServerData sd;
    sd.setAvailableLanguages({"en", "de"});

    ASSERT_TRUE(sd.addRedirect("/something", "https://example.com/path"));
    const auto match = sd.findMatchingRedirect("/de/something");

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->first, "https://example.com/path");
}
