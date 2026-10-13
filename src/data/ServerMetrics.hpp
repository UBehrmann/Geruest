/**
 * @file ServerMetrics.hpp
 * @brief Request counters, per-minute rolling windows, and latency histograms for /status.
 */

#ifndef GERUEST_SERVERMETRICS_HPP
#define GERUEST_SERVERMETRICS_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace geruest {

class JSONParser;

class ServerMetrics {
   public:
    /** Lifetime counters (persisted across restarts). */
    enum class Counter : std::size_t {
        Requests,
        Status2xx,
        Status3xx,
        Status4xx,
        Status5xx,
        InternalErrors,
        Connections,
        QueueRejections,
        AcceptErrors,
        AcceptEmfile,
        FramingRejects,
        WriteFailures,
        FileOpenFailures,
        StatusAuthFailures,
        WebSocketUpgrades,
        Count_
    };
    static constexpr std::size_t kCounterCount = static_cast<std::size_t>(Counter::Count_);

    /** Latency histogram bins: upper bounds grow by sqrt(2) from 50 µs; the last bin is open-ended. */
    static constexpr std::size_t kLatBins = 44;

    ServerMetrics() = default;
    ServerMetrics(const ServerMetrics&) = delete;
    ServerMetrics& operator=(const ServerMetrics&) = delete;

    static bool isMetricsExcludedPath(const std::string& path);

    void add(Counter c) const;
    uint64_t get(Counter c) const;

    /** Counts a response by status class (2xx–5xx); other codes (0 = nothing sent, 1xx) are ignored. */
    void recordStatus(int code) const;
    void recordLatency(uint32_t us) const;
    /** Sampled once per second by the server; feeds the time-weighted average queue fill. */
    void recordQueueFill(float fillPct) const;

    void adjustActiveRequests(int delta) const;
    void adjustActiveWebSockets(int delta) const;
    int64_t getActiveRequests() const;
    int64_t getActiveWebSockets() const;

    struct Window {
        uint64_t requests = 0;
        uint64_t errors_4xx = 0;
        uint64_t errors_5xx = 0;
        uint64_t errors_int = 0;
        uint64_t rejections = 0;
        double avg_queue_fill = 0.0;
        /** Approximate percentiles in milliseconds (histogram interpolation); 0 when no samples. */
        double p50 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
    };

    /** Aggregate over the last @p minutes minute buckets, including the current (partial) minute. */
    Window getWindow(uint32_t minutes) const;

    uint64_t getUptimeSeconds() const;
    double getUptimeHoursTotal() const;

    bool loadPersistentMetricsFromFile(const std::string& path);
    bool savePersistentMetricsToFile(const std::string& path) const;

   private:
    struct MinuteBucket {
        uint32_t epoch = 0;
        uint32_t requests = 0;
        uint32_t errors_4xx = 0;
        uint32_t errors_5xx = 0;
        uint32_t errors_int = 0;
        uint32_t rejections = 0;
        float fill_sum = 0.f;
        uint32_t fill_count = 0;
        std::array<uint32_t, kLatBins> lat{};
    };

    static uint32_t nowMinute_();
    MinuteBucket& currentBucket_() const;
    void clearAllMetrics_();
    bool importPersistentMetricsJson_(JSONParser root);

    mutable std::array<std::atomic<uint64_t>, kCounterCount> _counters{};
    mutable std::atomic<int64_t> _activeRequests{0};
    mutable std::atomic<int64_t> _activeWebSockets{0};
    std::chrono::steady_clock::time_point _startTime{std::chrono::steady_clock::now()};
    uint64_t _lifetimeUptimeBaselineSeconds{0};

    mutable std::mutex _metricsMutex;
    mutable std::array<MinuteBucket, 60> _minBuckets{};
};

}  // namespace geruest

#endif  // GERUEST_SERVERMETRICS_HPP
