/**
 * @file ServerMetrics.hpp
 * @brief Request counters, rolling windows, and latency tracking.
 */

#ifndef GERUEST_SERVERMETRICS_HPP
#define GERUEST_SERVERMETRICS_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace geruest {

class JSONParser;

class ServerMetrics {
   public:
    ServerMetrics() = default;
    ServerMetrics(const ServerMetrics&) = delete;
    ServerMetrics& operator=(const ServerMetrics&) = delete;

    static bool isMetricsExcludedPath(const std::string& path);

    void recordRequest() const;
    void recordError() const;
    void record4xx() const;
    void record5xx() const;
    void recordQueueRejection() const;
    void recordAcceptError() const;
    void recordAcceptEmfile() const;
    void recordFileOpenFailure() const;
    void recordOverloadHttpResponse() const;
    void recordQueueFill(float fillPct) const;
    void incrementActiveHandlers() const;
    void decrementActiveHandlers() const;
    void recordLatency(uint32_t us) const;

    uint64_t getTotalRequests() const;
    uint64_t getTotalErrors() const;
    uint64_t getTotal4xx() const;
    uint64_t getTotal5xx() const;
    uint64_t getTotalInternalErrors() const;
    uint64_t getQueueRejections() const;
    uint64_t getAcceptErrorsTotal() const;
    uint64_t getAcceptEmfileTotal() const;
    uint64_t getFileOpenFailures() const;
    uint64_t getOverloadHttpResponses() const;
    int64_t getActiveHandlers() const;

    struct WindowMetrics {
        uint64_t requests = 0;
        uint64_t errors_4xx = 0;
        uint64_t errors_5xx = 0;
        uint64_t errors_int = 0;
        double avg_queue_fill = 0.0;
    };

    WindowMetrics getWindowMetricsHour() const;
    WindowMetrics getRollingAveragePerHour() const;

    struct LatencyStats {
        double p50 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
    };

    LatencyStats getLatencyStats(uint32_t windowSeconds) const;

    uint64_t getUptimeSeconds() const;
    double getUptimeHoursTotal() const;

    bool loadPersistentMetricsFromFile(const std::string& path);
    bool savePersistentMetricsToFile(const std::string& path) const;

   private:
    struct RollingBucket {
        uint32_t epoch = 0;
        uint32_t requests = 0;
        uint32_t errors_4xx = 0;
        uint32_t errors_5xx = 0;
        uint32_t errors_int = 0;
        float fill_sum = 0.f;
        uint32_t fill_count = 0;
    };

    struct LatencySample {
        uint32_t epoch_s = 0;
        uint32_t us = 0;
    };

    static constexpr size_t kLatCap = 10000;

    void clearAllMetrics_();
    bool importPersistentMetricsJson_(JSONParser root);

    void writeBuckets_(uint32_t epochS, uint32_t epochM, uint32_t req, uint32_t e4, uint32_t e5, uint32_t ei,
                       float qFill, uint32_t qCnt) const;

    static std::pair<uint32_t, uint32_t> nowEpochs_();

    mutable std::atomic<uint64_t> _totalRequests{0};
    mutable std::atomic<uint64_t> _total4xx{0};
    mutable std::atomic<uint64_t> _total5xx{0};
    mutable std::atomic<uint64_t> _totalInternalErrors{0};
    mutable std::atomic<uint64_t> _queueRejections{0};
    mutable std::atomic<uint64_t> _acceptErrorsTotal{0};
    mutable std::atomic<uint64_t> _acceptEmfileTotal{0};
    mutable std::atomic<uint64_t> _fileOpenFailures{0};
    mutable std::atomic<uint64_t> _overloadHttpResponses{0};
    mutable std::atomic<int64_t> _activeHandlers{0};
    std::chrono::steady_clock::time_point _startTime{std::chrono::steady_clock::now()};
    uint64_t _lifetimeUptimeBaselineSeconds{0};

    mutable std::mutex _metricsMutex;
    mutable std::array<RollingBucket, 60> _minBuckets{};
    mutable std::array<LatencySample, kLatCap> _latSamples{};
    mutable size_t _latHead{0};
    mutable size_t _latCount{0};
};

}  // namespace geruest

#endif  // GERUEST_SERVERMETRICS_HPP
