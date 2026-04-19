#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "data/ServerData.hpp"
#include "parser/JSONParser.hpp"

using namespace geruest;

namespace fs = std::filesystem;

static fs::path uniqueTempJson(const char* base) {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / (std::string("geruest_sd_") + base + "_" + std::to_string(tick) + ".json");
}

TEST(ServerDataMetricsPersistence, MissingFileLoadSucceeds) {
    ServerData sd;
    const fs::path p = uniqueTempJson("missing");
    ASSERT_TRUE(sd.loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(sd.getTotalRequests(), 0u);
    EXPECT_EQ(sd.getUptimeHoursTotal(), 0.0);
    if (fs::exists(p)) {
        fs::remove(p);
    }
}

TEST(ServerDataMetricsPersistence, InvalidFileReturnsFalse) {
    const fs::path p = uniqueTempJson("bad");
    {
        std::ofstream f(p);
        f << "{\"not\":\"metrics\"}";
    }
    ServerData sd;
    sd.recordRequest();
    ASSERT_FALSE(sd.loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(sd.getTotalRequests(), 0u);
    fs::remove(p);
}

TEST(ServerDataMetricsPersistence, RoundTripCountersAndLifetime) {
    const fs::path p = uniqueTempJson("roundtrip");

    {
        ServerData a;
        a.recordRequest();
        a.recordRequest();
        a.record5xx();
        a.recordQueueRejection();
        ASSERT_TRUE(a.savePersistentMetricsToFile(p.string()));
    }

    ServerData b;
    ASSERT_TRUE(b.loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(b.getTotalRequests(), 2u);
    EXPECT_EQ(b.getTotal5xx(), 1u);
    EXPECT_EQ(b.getQueueRejections(), 1u);

    // loadPersistentMetricsFromFile resets session start; Windows CI can wake
    // slightly under one wall-clock second for duration_cast<seconds>.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (b.getUptimeSeconds() < 1u && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_GE(b.getUptimeSeconds(), 1u);

    ASSERT_TRUE(b.savePersistentMetricsToFile(p.string()));

    ServerData c;
    ASSERT_TRUE(c.loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(c.getTotalRequests(), 2u);

    fs::remove(p);
}

TEST(ServerDataMetricsPersistence, SaveJSONToFileAtomicLeavesNoTmp) {
    const fs::path p = uniqueTempJson("atomic");
    const fs::path tmp = p.string() + ".tmp";

    JSONParser j;
    j.setString("k", "v");
    ASSERT_TRUE(saveJSONToFileAtomic(j, p.string()));
    EXPECT_TRUE(fs::exists(p));
    EXPECT_FALSE(fs::exists(tmp));
    auto parsed = getJSONFromFileSafe(p.string());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getString("k"), "v");
    fs::remove(p);
}
