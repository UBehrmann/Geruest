#include "ServerMetrics.hpp"

#include "parser/JSONParser.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace geruest {

namespace {

constexpr int kMetricsSchemaVersion = 1;

uint64_t parseJsonU64(JSONParser& j, const std::string& key, uint64_t fallback) {
    if (!j.hasKey(key)) {
        return fallback;
    }
    const std::string s = j.getString(key);
    if (!s.empty()) {
        bool allDigit = true;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigit = false;
                break;
            }
        }
        if (allDigit) {
            try {
                return std::stoull(s);
            } catch (...) {
                return fallback;
            }
        }
    }
    const long long v = j.getLongLong(key);
    if (v < 0) {
        return fallback;
    }
    return static_cast<uint64_t>(v);
}

void appendIsoUtcNow(JSONParser& root) {
    std::time_t now_t = std::time(nullptr);
    char timeBuf[32] = {};
    struct tm utcTm{};
    gmtime_r(&now_t, &utcTm);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
    root.setString("saved_at", timeBuf);
}

}  // namespace

bool ServerMetrics::isMetricsExcludedPath(const std::string& path) {
    return path == "/status";
}

std::pair<uint32_t, uint32_t> ServerMetrics::nowEpochs_() {
    const auto now = std::chrono::system_clock::now();
    const uint32_t es =
        static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    return {es, es / 60};
}

void ServerMetrics::writeBuckets_(uint32_t epochS, uint32_t epochM, uint32_t req, uint32_t e4, uint32_t e5,
                                  uint32_t ei, float qFill, uint32_t qCnt) const {
    (void)epochS;
    auto apply = [](RollingBucket& b, uint32_t ep, uint32_t req_, uint32_t e4_, uint32_t e5_, uint32_t ei_,
                    float qFill_, uint32_t qCnt_) {
        if (b.epoch != ep) {
            b = RollingBucket{ep, 0, 0, 0, 0, 0.f, 0};
        }
        b.requests += req_;
        b.errors_4xx += e4_;
        b.errors_5xx += e5_;
        b.errors_int += ei_;
        b.fill_sum += qFill_;
        b.fill_count += qCnt_;
    };
    apply(_minBuckets[epochM % 60], epochM, req, e4, e5, ei, qFill, qCnt);
}

void ServerMetrics::recordRequest() const {
    _totalRequests.fetch_add(1, std::memory_order_relaxed);
    const auto ep = nowEpochs_();
    std::lock_guard<std::mutex> lock(_metricsMutex);
    writeBuckets_(ep.first, ep.second, 1, 0, 0, 0, 0.f, 0);
}

void ServerMetrics::recordError() const {
    _totalInternalErrors.fetch_add(1, std::memory_order_relaxed);
    const auto ep = nowEpochs_();
    std::lock_guard<std::mutex> lock(_metricsMutex);
    writeBuckets_(ep.first, ep.second, 0, 0, 0, 1, 0.f, 0);
}

void ServerMetrics::record4xx() const {
    _total4xx.fetch_add(1, std::memory_order_relaxed);
    const auto ep = nowEpochs_();
    std::lock_guard<std::mutex> lock(_metricsMutex);
    writeBuckets_(ep.first, ep.second, 0, 1, 0, 0, 0.f, 0);
}

void ServerMetrics::record5xx() const {
    _total5xx.fetch_add(1, std::memory_order_relaxed);
    const auto ep = nowEpochs_();
    std::lock_guard<std::mutex> lock(_metricsMutex);
    writeBuckets_(ep.first, ep.second, 0, 0, 1, 0, 0.f, 0);
}

void ServerMetrics::recordQueueRejection() const {
    _queueRejections.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::recordAcceptError() const {
    _acceptErrorsTotal.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::recordAcceptEmfile() const {
    _acceptEmfileTotal.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::recordFileOpenFailure() const {
    _fileOpenFailures.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::recordOverloadHttpResponse() const {
    _overloadHttpResponses.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::recordQueueFill(float fillPct) const {
    const auto ep = nowEpochs_();
    std::lock_guard<std::mutex> lock(_metricsMutex);
    writeBuckets_(ep.first, ep.second, 0, 0, 0, 0, fillPct, 1);
}

void ServerMetrics::incrementActiveHandlers() const {
    _activeHandlers.fetch_add(1, std::memory_order_relaxed);
}

void ServerMetrics::decrementActiveHandlers() const {
    _activeHandlers.fetch_sub(1, std::memory_order_relaxed);
}

void ServerMetrics::recordLatency(uint32_t us) const {
    const auto ep = nowEpochs_();
    std::lock_guard<std::mutex> lock(_metricsMutex);
    _latSamples[_latHead] = {ep.first, us};
    _latHead = (_latHead + 1) % kLatCap;
    if (_latCount < kLatCap) {
        ++_latCount;
    }
}

uint64_t ServerMetrics::getTotalRequests() const {
    return _totalRequests.load(std::memory_order_relaxed);
}

uint64_t ServerMetrics::getTotalErrors() const {
    return _total4xx.load(std::memory_order_relaxed) + _total5xx.load(std::memory_order_relaxed) +
           _totalInternalErrors.load(std::memory_order_relaxed);
}

uint64_t ServerMetrics::getTotal4xx() const { return _total4xx.load(std::memory_order_relaxed); }
uint64_t ServerMetrics::getTotal5xx() const { return _total5xx.load(std::memory_order_relaxed); }
uint64_t ServerMetrics::getTotalInternalErrors() const {
    return _totalInternalErrors.load(std::memory_order_relaxed);
}
uint64_t ServerMetrics::getQueueRejections() const { return _queueRejections.load(std::memory_order_relaxed); }
uint64_t ServerMetrics::getAcceptErrorsTotal() const { return _acceptErrorsTotal.load(std::memory_order_relaxed); }
uint64_t ServerMetrics::getAcceptEmfileTotal() const { return _acceptEmfileTotal.load(std::memory_order_relaxed); }
uint64_t ServerMetrics::getFileOpenFailures() const { return _fileOpenFailures.load(std::memory_order_relaxed); }
uint64_t ServerMetrics::getOverloadHttpResponses() const {
    return _overloadHttpResponses.load(std::memory_order_relaxed);
}
int64_t ServerMetrics::getActiveHandlers() const { return _activeHandlers.load(std::memory_order_relaxed); }

ServerMetrics::WindowMetrics ServerMetrics::getWindowMetricsHour() const {
    const auto ep = nowEpochs_();
    const uint32_t curM = ep.second;
    std::lock_guard<std::mutex> lock(_metricsMutex);
    WindowMetrics wm;
    double fillSum = 0.0;
    uint64_t fillN = 0;
    for (const auto& b : _minBuckets) {
        if (b.epoch > 0 && curM >= b.epoch && (curM - b.epoch) < 60) {
            wm.requests += b.requests;
            wm.errors_4xx += b.errors_4xx;
            wm.errors_5xx += b.errors_5xx;
            wm.errors_int += b.errors_int;
            fillSum += b.fill_sum;
            fillN += b.fill_count;
        }
    }
    if (fillN > 0) {
        wm.avg_queue_fill = fillSum / static_cast<double>(fillN);
    }
    return wm;
}

ServerMetrics::WindowMetrics ServerMetrics::getRollingAveragePerHour() const {
    const uint64_t uptime = getUptimeSeconds();
    const uint64_t hours = uptime / 3600;
    if (hours == 0) {
        return getWindowMetricsHour();
    }
    std::lock_guard<std::mutex> lock(_metricsMutex);
    WindowMetrics wm;
    double fillSum = 0.0;
    uint64_t fillN = 0;
    for (const auto& b : _minBuckets) {
        wm.requests += b.requests;
        wm.errors_4xx += b.errors_4xx;
        wm.errors_5xx += b.errors_5xx;
        wm.errors_int += b.errors_int;
        fillSum += b.fill_sum;
        fillN += b.fill_count;
    }
    if (fillN > 0) {
        wm.avg_queue_fill = fillSum / static_cast<double>(fillN);
    }
    wm.requests = hours ? wm.requests / hours : wm.requests;
    wm.errors_4xx = hours ? wm.errors_4xx / hours : wm.errors_4xx;
    wm.errors_5xx = hours ? wm.errors_5xx / hours : wm.errors_5xx;
    wm.errors_int = hours ? wm.errors_int / hours : wm.errors_int;
    return wm;
}

ServerMetrics::LatencyStats ServerMetrics::getLatencyStats(uint32_t windowSeconds) const {
    const auto ep = nowEpochs_();
    const uint32_t curS = ep.first;
    const uint32_t cutoff = (curS > windowSeconds) ? (curS - windowSeconds) : 0;
    std::lock_guard<std::mutex> lock(_metricsMutex);
    std::vector<uint32_t> relevant;
    relevant.reserve(_latCount);
    const size_t start = (_latHead + kLatCap - _latCount) % kLatCap;
    for (size_t i = 0; i < _latCount; ++i) {
        const LatencySample& s = _latSamples[(start + i) % kLatCap];
        if (s.epoch_s >= cutoff) {
            relevant.push_back(s.us);
        }
    }
    if (relevant.empty()) {
        return {};
    }
    std::sort(relevant.begin(), relevant.end());
    const size_t n = relevant.size();
    return {relevant[n * 50 / 100] / 1000.0, relevant[n * 95 / 100] / 1000.0,
            relevant[n * 99 / 100] / 1000.0};
}

uint64_t ServerMetrics::getUptimeSeconds() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _startTime).count());
}

double ServerMetrics::getUptimeHoursTotal() const {
    return static_cast<double>(_lifetimeUptimeBaselineSeconds + getUptimeSeconds()) / 3600.0;
}

void ServerMetrics::clearAllMetrics_() {
    _totalRequests.store(0, std::memory_order_relaxed);
    _total4xx.store(0, std::memory_order_relaxed);
    _total5xx.store(0, std::memory_order_relaxed);
    _totalInternalErrors.store(0, std::memory_order_relaxed);
    _queueRejections.store(0, std::memory_order_relaxed);
    _acceptErrorsTotal.store(0, std::memory_order_relaxed);
    _acceptEmfileTotal.store(0, std::memory_order_relaxed);
    _fileOpenFailures.store(0, std::memory_order_relaxed);
    _overloadHttpResponses.store(0, std::memory_order_relaxed);
    _activeHandlers.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(_metricsMutex);
    _minBuckets.fill({});
    _latHead = 0;
    _latCount = 0;
    _latSamples.fill({});
}

bool ServerMetrics::importPersistentMetricsJson_(JSONParser root) {
    if (!root.hasKey("schema_version") || root.getInt("schema_version") != kMetricsSchemaVersion) {
        return false;
    }

    const std::vector<JSONParser> bucketObjs = root.getArrayOfJSON("buckets");
    if (bucketObjs.size() != 60) {
        return false;
    }

    JSONParser latRoot;
    if (root.hasKey("latency")) {
        latRoot = root.getObject("latency");
    }
    const std::vector<JSONParser> latSamples =
        latRoot.hasKey("samples") ? latRoot.getArrayOfJSON("samples") : std::vector<JSONParser>{};
    const uint64_t latCount = latRoot.hasKey("count") ? parseJsonU64(latRoot, "count", 0) : 0;
    if (latCount > kLatCap) {
        return false;
    }
    if (latSamples.size() != latCount) {
        return false;
    }

    const uint64_t baseline = parseJsonU64(root, "lifetime_uptime_seconds", 0);

    const uint64_t tr = parseJsonU64(root, "total_requests", 0);
    const uint64_t t4 = parseJsonU64(root, "total_4xx", 0);
    const uint64_t t5 = parseJsonU64(root, "total_5xx", 0);
    const uint64_t ti = parseJsonU64(root, "total_internal_errors", 0);
    const uint64_t qr = parseJsonU64(root, "queue_rejections", 0);

    std::array<RollingBucket, 60> buckets{};
    for (size_t i = 0; i < 60; ++i) {
        JSONParser b = bucketObjs[i];
        buckets[i].epoch = static_cast<uint32_t>(parseJsonU64(b, "epoch", 0));
        buckets[i].requests = static_cast<uint32_t>(parseJsonU64(b, "requests", 0));
        buckets[i].errors_4xx = static_cast<uint32_t>(parseJsonU64(b, "errors_4xx", 0));
        buckets[i].errors_5xx = static_cast<uint32_t>(parseJsonU64(b, "errors_5xx", 0));
        buckets[i].errors_int = static_cast<uint32_t>(parseJsonU64(b, "errors_int", 0));
        buckets[i].fill_sum = b.hasKey("fill_sum") ? b.getFloat("fill_sum") : 0.f;
        buckets[i].fill_count = static_cast<uint32_t>(parseJsonU64(b, "fill_count", 0));
    }

    std::array<LatencySample, kLatCap> latBuf{};
    for (uint64_t i = 0; i < latCount; ++i) {
        JSONParser s = latSamples[static_cast<size_t>(i)];
        latBuf[static_cast<size_t>(i)].epoch_s = static_cast<uint32_t>(parseJsonU64(s, "epoch_s", 0));
        latBuf[static_cast<size_t>(i)].us = static_cast<uint32_t>(parseJsonU64(s, "us", 0));
    }

    _totalRequests.store(tr, std::memory_order_relaxed);
    _total4xx.store(t4, std::memory_order_relaxed);
    _total5xx.store(t5, std::memory_order_relaxed);
    _totalInternalErrors.store(ti, std::memory_order_relaxed);
    _queueRejections.store(qr, std::memory_order_relaxed);
    _activeHandlers.store(0, std::memory_order_relaxed);
    _lifetimeUptimeBaselineSeconds = baseline;

    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _minBuckets = buckets;
        _latCount = static_cast<size_t>(latCount);
        _latSamples = latBuf;
        _latHead = latCount > 0 ? (static_cast<size_t>(latCount) % kLatCap) : 0;
    }

    return true;
}

bool ServerMetrics::loadPersistentMetricsFromFile(const std::string& path) {
    const auto resetSessionStart = [this] { _startTime = std::chrono::steady_clock::now(); };

    std::unique_ptr<JSONParser> parsed = getJSONFromFileSafe(path);
    if (!parsed) {
        _lifetimeUptimeBaselineSeconds = 0;
        resetSessionStart();
        return true;
    }

    if (!importPersistentMetricsJson_(std::move(*parsed))) {
        clearAllMetrics_();
        _lifetimeUptimeBaselineSeconds = 0;
        resetSessionStart();
        return false;
    }

    resetSessionStart();
    return true;
}

bool ServerMetrics::savePersistentMetricsToFile(const std::string& path) const {
    std::array<RollingBucket, 60> bucketsCopy{};
    std::array<LatencySample, kLatCap> latCopy{};
    size_t latHeadCopy = 0;
    size_t latCountCopy = 0;
    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        bucketsCopy = _minBuckets;
        latCopy = _latSamples;
        latHeadCopy = _latHead;
        latCountCopy = _latCount;
    }

    const uint64_t tr = _totalRequests.load(std::memory_order_relaxed);
    const uint64_t t4 = _total4xx.load(std::memory_order_relaxed);
    const uint64_t t5 = _total5xx.load(std::memory_order_relaxed);
    const uint64_t ti = _totalInternalErrors.load(std::memory_order_relaxed);
    const uint64_t qr = _queueRejections.load(std::memory_order_relaxed);

    const uint64_t sessionSec = getUptimeSeconds();
    const uint64_t fileLifetime = _lifetimeUptimeBaselineSeconds + sessionSec;

    JSONParser root;
    root.setInt("schema_version", kMetricsSchemaVersion);
    appendIsoUtcNow(root);
    root.setString("lifetime_uptime_seconds", std::to_string(fileLifetime));
    root.setLongLong("total_requests", static_cast<long long>(tr));
    root.setLongLong("total_4xx", static_cast<long long>(t4));
    root.setLongLong("total_5xx", static_cast<long long>(t5));
    root.setLongLong("total_internal_errors", static_cast<long long>(ti));
    root.setLongLong("queue_rejections", static_cast<long long>(qr));

    std::vector<JSONParser> bucketJson;
    bucketJson.reserve(60);
    for (const RollingBucket& b : bucketsCopy) {
        JSONParser o;
        o.setLongLong("epoch", static_cast<long long>(b.epoch));
        o.setLongLong("requests", static_cast<long long>(b.requests));
        o.setLongLong("errors_4xx", static_cast<long long>(b.errors_4xx));
        o.setLongLong("errors_5xx", static_cast<long long>(b.errors_5xx));
        o.setLongLong("errors_int", static_cast<long long>(b.errors_int));
        o.setDouble("fill_sum", static_cast<double>(b.fill_sum));
        o.setLongLong("fill_count", static_cast<long long>(b.fill_count));
        bucketJson.push_back(std::move(o));
    }
    root.setArrayOfJSON("buckets", bucketJson);

    JSONParser latRoot;
    latRoot.setLongLong("head", static_cast<long long>(latHeadCopy));
    latRoot.setLongLong("count", static_cast<long long>(latCountCopy));
    std::vector<JSONParser> samplesJson;
    samplesJson.reserve(latCountCopy);
    if (latCountCopy > 0) {
        const size_t start = (latHeadCopy + kLatCap - latCountCopy) % kLatCap;
        for (size_t i = 0; i < latCountCopy; ++i) {
            const LatencySample& s = latCopy[(start + i) % kLatCap];
            JSONParser sj;
            sj.setLongLong("epoch_s", static_cast<long long>(s.epoch_s));
            sj.setLongLong("us", static_cast<long long>(s.us));
            samplesJson.push_back(std::move(sj));
        }
    }
    latRoot.setArrayOfJSON("samples", samplesJson);
    root.setJSON("latency", latRoot);

    return saveJSONToFileAtomic(root, path);
}

}  // namespace geruest
