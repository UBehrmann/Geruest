#include <gtest/gtest.h>

#include "data/SiteRevision.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST(SiteRevision, Fnv1aHexStable) {
    EXPECT_EQ(geruest::fnv1a64Hex("hello"), geruest::fnv1a64Hex("hello"));
    EXPECT_NE(geruest::fnv1a64Hex("hello"), geruest::fnv1a64Hex("world"));
}

TEST(SiteRevision, ComputeChangesWhenFileChanges) {
    const fs::path dir = fs::temp_directory_path() / "geruest_site_revision_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    ASSERT_TRUE(fs::exists(dir));

    const fs::path file = dir / "a.txt";
    {
        std::ofstream out(file);
        out << "v1";
    }

    const std::string first = geruest::computeSiteRevision(dir.string());
    EXPECT_NE(first, "0");

    {
        std::ofstream out(dir / "b.txt");
        out << "new";
    }

    const std::string second = geruest::computeSiteRevision(dir.string());
    EXPECT_NE(first, second);

    fs::remove_all(dir, ec);
}
