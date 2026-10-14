#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "data/ServerData.hpp"
#include "parser/JSONParser.hpp"

using namespace geruest;
using Counter = ServerMetrics::Counter;

namespace fs = std::filesystem;

static fs::path uniqueTempJson(const char* base) {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / (std::string("geruest_sd_") + base + "_" + std::to_string(tick) + ".json");
}

TEST(ServerDataMetricsPersistence, MissingFileLoadSucceeds) {
    ServerData sd;
    const fs::path p = uniqueTempJson("missing");
    ASSERT_TRUE(sd.metrics().loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(sd.metrics().get(Counter::Requests), 0u);
    EXPECT_EQ(sd.metrics().getUptimeHoursTotal(), 0.0);
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
    sd.metrics().add(Counter::Requests);
    ASSERT_FALSE(sd.metrics().loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(sd.metrics().get(Counter::Requests), 0u);
    fs::remove(p);
}

TEST(ServerDataMetricsPersistence, RoundTripCountersAndLifetime) {
    const fs::path p = uniqueTempJson("roundtrip");

    {
        ServerData a;
        const ServerMetrics& m = a.metrics();
        m.add(Counter::Requests);
        m.add(Counter::Requests);
        m.recordStatus(500);
        m.add(Counter::QueueRejections);
        m.add(Counter::WriteFailures);
        m.recordLatency(1000);
        ASSERT_TRUE(a.metrics().savePersistentMetricsToFile(p.string()));
    }

    ServerData b;
    ASSERT_TRUE(b.metrics().loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(b.metrics().get(Counter::Requests), 2u);
    EXPECT_EQ(b.metrics().get(Counter::Status5xx), 1u);
    EXPECT_EQ(b.metrics().get(Counter::QueueRejections), 1u);
    EXPECT_EQ(b.metrics().get(Counter::WriteFailures), 1u);
    EXPECT_EQ(b.metrics().getWindow(60).requests, 2u);
    EXPECT_GT(b.metrics().getWindow(60).p50, 0.0);

    // loadPersistentMetricsFromFile resets session start; Windows CI can wake
    // slightly under one wall-clock second for duration_cast<seconds>.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (b.metrics().getUptimeSeconds() < 1u && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_GE(b.metrics().getUptimeSeconds(), 1u);

    ASSERT_TRUE(b.metrics().savePersistentMetricsToFile(p.string()));

    ServerData c;
    ASSERT_TRUE(c.metrics().loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(c.metrics().get(Counter::Requests), 2u);

    fs::remove(p);
}

TEST(ServerDataMetricsPersistence, LoadsSchemaVersion1) {
    const fs::path p = uniqueTempJson("v1");
    {
        JSONParser root;
        root.setInt("schema_version", 1);
        root.setString("lifetime_uptime_seconds", "7200");
        root.setLongLong("total_requests", 10);
        root.setLongLong("total_4xx", 3);
        std::vector<JSONParser> buckets(60);
        for (JSONParser& b : buckets) {
            b.setLongLong("epoch", 0);
        }
        root.setArrayOfJSON("buckets", buckets);
        ASSERT_TRUE(saveJSONToFileAtomic(root, p.string()));
    }
    ServerData sd;
    ASSERT_TRUE(sd.metrics().loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(sd.metrics().get(Counter::Requests), 10u);
    EXPECT_EQ(sd.metrics().get(Counter::Status4xx), 3u);
    EXPECT_NEAR(sd.metrics().getUptimeHoursTotal(), 2.0, 0.01);
    fs::remove(p);
}

TEST(ServerMetrics, StatusClassesAndWindow) {
    ServerMetrics m;
    m.recordStatus(200);
    m.recordStatus(204);
    m.recordStatus(301);
    m.recordStatus(404);
    m.recordStatus(503);
    m.recordStatus(101);  // ignored
    m.recordStatus(0);    // nothing sent: ignored
    m.add(Counter::QueueRejections);
    EXPECT_EQ(m.get(Counter::Status2xx), 2u);
    EXPECT_EQ(m.get(Counter::Status3xx), 1u);
    EXPECT_EQ(m.get(Counter::Status4xx), 1u);
    EXPECT_EQ(m.get(Counter::Status5xx), 1u);

    const ServerMetrics::Window w = m.getWindow(5);
    EXPECT_EQ(w.errors_4xx, 1u);
    EXPECT_EQ(w.errors_5xx, 1u);
    EXPECT_EQ(w.rejections, 1u);
}

TEST(ServerMetrics, LatencyPercentilesFromHistogram) {
    ServerMetrics m;
    for (int i = 0; i < 90; ++i) {
        m.recordLatency(1000);  // 1 ms
    }
    for (int i = 0; i < 10; ++i) {
        m.recordLatency(100000);  // 100 ms
    }
    const ServerMetrics::Window w = m.getWindow(5);
    // Bins are sqrt(2) wide, so estimates land within ~40% of the true value.
    EXPECT_GT(w.p50, 0.6);
    EXPECT_LT(w.p50, 1.5);
    EXPECT_GT(w.p95, 60.0);
    EXPECT_LT(w.p95, 150.0);
    EXPECT_GE(w.p99, w.p95);

    EXPECT_EQ(ServerMetrics().getWindow(5).p99, 0.0);
}

TEST(ServerMetrics, QueueFillAverage) {
    ServerMetrics m;
    m.recordQueueFill(0.f);
    m.recordQueueFill(50.f);
    m.recordQueueFill(250.f);  // clamped to 100
    EXPECT_NEAR(m.getWindow(5).avg_queue_fill, 50.0, 0.01);
}

TEST(ServerDataMetricsPersistence, ErrorLogCapsAndRoundTrips) {
    const fs::path p = uniqueTempJson("errorlog");
    const size_t extra = 5;
    {
        ServerData a;
        for (size_t i = 0; i < ServerMetrics::kErrorLogDefaultCapacity + extra; ++i) {
            ServerMetrics::ErrorEntry e;
            e.status = 404;
            e.ip = "203.0.113.7";
            e.request = "GET /missing-" + std::to_string(i) + ".png HTTP/1.1";
            e.referer = std::string(ServerMetrics::kErrorFieldMaxBytes + 10, 'r');
            e.userAgent = R"(bot "quoted" \ back)";
            a.metrics().recordError(e);
        }
        const auto all = a.metrics().getLastErrors(SIZE_MAX);
        ASSERT_EQ(all.size(), ServerMetrics::kErrorLogDefaultCapacity);
        EXPECT_EQ(all.front().seq, extra + 1);
        EXPECT_EQ(all.back().referer.size(), ServerMetrics::kErrorFieldMaxBytes);
        ASSERT_TRUE(a.metrics().savePersistentMetricsToFile(p.string()));
    }

    ServerData b;
    ASSERT_TRUE(b.metrics().loadPersistentMetricsFromFile(p.string()));
    const uint64_t total = ServerMetrics::kErrorLogDefaultCapacity + extra;
    EXPECT_EQ(b.metrics().lastErrorSeq(), total);
    const auto tail = b.metrics().getLastErrors(1);
    ASSERT_EQ(tail.size(), 1u);
    EXPECT_EQ(tail[0].seq, total);
    EXPECT_EQ(tail[0].status, 404);
    EXPECT_EQ(tail[0].ip, "203.0.113.7");
    EXPECT_EQ(tail[0].request, "GET /missing-" + std::to_string(total - 1) + ".png HTTP/1.1");
    EXPECT_EQ(tail[0].userAgent, R"(bot "quoted" \ back)");
    EXPECT_GT(tail[0].ts, 0);

    ServerMetrics::ErrorEntry next;
    next.status = 500;
    b.metrics().recordError(next);
    EXPECT_EQ(b.metrics().lastErrorSeq(), total + 1);

    // Shrinking keeps the newest entries; a smaller capacity also trims on load.
    b.metrics().setErrorLogCapacity(10);
    const auto ten = b.metrics().getLastErrors(SIZE_MAX);
    ASSERT_EQ(ten.size(), 10u);
    EXPECT_EQ(ten.back().seq, total + 1);
    ServerData c;
    c.metrics().setErrorLogCapacity(3);
    ASSERT_TRUE(c.metrics().loadPersistentMetricsFromFile(p.string()));
    EXPECT_EQ(c.metrics().getLastErrors(SIZE_MAX).size(), 3u);

    b.metrics().setErrorLogCapacity(0);
    EXPECT_TRUE(b.metrics().getLastErrors(SIZE_MAX).empty());
    b.metrics().recordError(next);
    EXPECT_TRUE(b.metrics().getLastErrors(SIZE_MAX).empty());

    b.metrics().setErrorLogCapacity(5'000'000);
    EXPECT_EQ(b.metrics().getErrorLogCapacity(), ServerMetrics::kErrorLogMaxCapacity);
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
