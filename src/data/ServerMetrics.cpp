#include "ServerMetrics.hpp"

#include "parser/JSONParser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace geruest {

namespace {

constexpr int kMetricsSchemaVersion = 2;

/** Persistence keys, indexed by ServerMetrics::Counter. The first five keep their schema-1 names. */
constexpr std::array<const char*, ServerMetrics::kCounterCount> kCounterKeys = {
    "total_requests",           "total_2xx",
    "total_3xx",                "total_4xx",
    "total_5xx",                "total_internal_errors",
    "connections_total",        "queue_rejections",
    "accept_errors_total",      "accept_emfile_total",
    "framing_rejects_total",    "write_failures_total",
    "file_open_failures_total", "status_auth_failures_total",
    "websocket_upgrades_total",
};

const std::array<uint32_t, ServerMetrics::kLatBins>& latencyBinUpperBoundsUs() {
    static const auto bounds = [] {
        std::array<uint32_t, ServerMetrics::kLatBins> b{};
        for (size_t i = 0; i < b.size(); ++i) {
            b[i] = static_cast<uint32_t>(std::lround(50.0 * std::pow(2.0, static_cast<double>(i) / 2.0)));
        }
        return b;
    }();
    return bounds;
}

size_t latencyBin(uint32_t us) {
    const auto& b = latencyBinUpperBoundsUs();
    const auto it = std::lower_bound(b.begin(), b.end(), us);
    return it == b.end() ? b.size() - 1 : static_cast<size_t>(it - b.begin());
}

/** Linear interpolation inside the bin holding the q-th sample; result in milliseconds. */
double histogramPercentileMs(const std::array<uint64_t, ServerMetrics::kLatBins>& h, uint64_t n, double q) {
    if (n == 0) {
        return 0.0;
    }
    const auto& b = latencyBinUpperBoundsUs();
    const double rank = std::max(1.0, std::ceil(q * static_cast<double>(n)));
    uint64_t cum = 0;
    for (size_t i = 0; i < h.size(); ++i) {
        if (h[i] == 0) {
            continue;
        }
        if (static_cast<double>(cum + h[i]) >= rank) {
            const double lo = i == 0 ? 0.0 : b[i - 1];
            const double hi = b[i];
            const double frac = (rank - static_cast<double>(cum)) / static_cast<double>(h[i]);
            return (lo + (hi - lo) * frac) / 1000.0;
        }
        cum += h[i];
    }
    return b.back() / 1000.0;
}

uint64_t parseJsonU64(JSONParser& j, const std::string& key, uint64_t fallback) {
    if (!j.hasKey(key)) {
        return fallback;
    }
    const std::string s = j.getString(key);
    if (!s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
        try {
            return std::stoull(s);
        } catch (...) {
            return fallback;
        }
    }
    const long long v = j.getLongLong(key);
    return v < 0 ? fallback : static_cast<uint64_t>(v);
}

std::string isoUtc(std::time_t t) {
    char timeBuf[32] = {};
    struct tm utcTm{};
    gmtime_r(&t, &utcTm);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
    return timeBuf;
}

void appendIsoUtcNow(JSONParser& root) {
    root.setString("saved_at", isoUtc(std::time(nullptr)));
}

ServerMetrics::ErrorEntry errorEntryFromJson(JSONParser& j) {
    ServerMetrics::ErrorEntry e;
    e.seq = parseJsonU64(j, "seq", 0);
    e.ts = static_cast<int64_t>(parseJsonU64(j, "ts", 0));
    e.status = j.hasKey("status") ? j.getInt("status") : 0;
    e.ip = j.getString("ip");
    e.forwardedFor = j.getString("forwarded_for");
    e.request = j.getString("request");
    e.host = j.getString("host");
    e.referer = j.getString("referer");
    e.userAgent = j.getString("user_agent");
    e.message = j.getString("message");
    return e;
}

}  // namespace

JSONParser ServerMetrics::errorEntryJson(const ErrorEntry& e) {
    JSONParser j;
    j.setLongLong("seq", static_cast<long long>(e.seq));
    j.setLongLong("ts", e.ts);
    j.setString("time", isoUtc(static_cast<std::time_t>(e.ts)));
    j.setInt("status", e.status);
    j.setString("ip", e.ip);
    j.setString("forwarded_for", e.forwardedFor);
    j.setString("request", e.request);
    j.setString("host", e.host);
    j.setString("referer", e.referer);
    j.setString("user_agent", e.userAgent);
    j.setString("message", e.message);
    return j;
}

bool ServerMetrics::isMetricsExcludedPath(const std::string& path) {
    return path == "/status" || path == "/status/errors";
}

void ServerMetrics::setErrorLogCapacity(std::size_t capacity) {
    std::lock_guard<std::mutex> lock(_errorMutex);
    _errorCapacity = std::min(capacity, kErrorLogMaxCapacity);
    while (_errors.size() > _errorCapacity) {
        _errors.pop_front();
    }
}

std::size_t ServerMetrics::getErrorLogCapacity() const {
    std::lock_guard<std::mutex> lock(_errorMutex);
    return _errorCapacity;
}

void ServerMetrics::recordError(ErrorEntry e) const {
    for (std::string* s : {&e.ip, &e.forwardedFor, &e.request, &e.host, &e.referer, &e.userAgent, &e.message}) {
        if (s->size() > kErrorFieldMaxBytes) {
            s->resize(kErrorFieldMaxBytes);
        }
    }
    e.ts = static_cast<int64_t>(std::time(nullptr));
    std::lock_guard<std::mutex> lock(_errorMutex);
    if (_errorCapacity == 0) {
        return;
    }
    e.seq = ++_errorSeq;
    _errors.push_back(std::move(e));
    while (_errors.size() > _errorCapacity) {
        _errors.pop_front();
    }
}

std::vector<ServerMetrics::ErrorEntry> ServerMetrics::getLastErrors(std::size_t count) const {
    std::lock_guard<std::mutex> lock(_errorMutex);
    return {_errors.end() - static_cast<std::ptrdiff_t>(std::min(count, _errors.size())), _errors.end()};
}

uint64_t ServerMetrics::lastErrorSeq() const {
    std::lock_guard<std::mutex> lock(_errorMutex);
    return _errorSeq;
}

uint32_t ServerMetrics::nowMinute_() {
    const auto now = std::chrono::system_clock::now();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::minutes>(now.time_since_epoch()).count());
}

ServerMetrics::MinuteBucket& ServerMetrics::currentBucket_() const {
    const uint32_t m = nowMinute_();
    MinuteBucket& b = _minBuckets[m % _minBuckets.size()];
    if (b.epoch != m) {
        b = MinuteBucket{};
        b.epoch = m;
    }
    return b;
}

void ServerMetrics::add(Counter c) const {
    _counters[static_cast<size_t>(c)].fetch_add(1, std::memory_order_relaxed);
    uint32_t MinuteBucket::*field = nullptr;
    switch (c) {
        case Counter::Requests: field = &MinuteBucket::requests; break;
        case Counter::Status4xx: field = &MinuteBucket::errors_4xx; break;
        case Counter::Status5xx: field = &MinuteBucket::errors_5xx; break;
        case Counter::InternalErrors: field = &MinuteBucket::errors_int; break;
        case Counter::QueueRejections: field = &MinuteBucket::rejections; break;
        default: return;
    }
    std::lock_guard<std::mutex> lock(_metricsMutex);
    ++(currentBucket_().*field);
}

uint64_t ServerMetrics::get(Counter c) const {
    return _counters[static_cast<size_t>(c)].load(std::memory_order_relaxed);
}

void ServerMetrics::recordStatus(int code) const {
    switch (code / 100) {
        case 2: add(Counter::Status2xx); break;
        case 3: add(Counter::Status3xx); break;
        case 4: add(Counter::Status4xx); break;
        case 5: add(Counter::Status5xx); break;
        default: break;
    }
}

void ServerMetrics::recordLatency(uint32_t us) const {
    const size_t bin = latencyBin(us);
    std::lock_guard<std::mutex> lock(_metricsMutex);
    ++currentBucket_().lat[bin];
}

void ServerMetrics::recordQueueFill(float fillPct) const {
    std::lock_guard<std::mutex> lock(_metricsMutex);
    MinuteBucket& b = currentBucket_();
    b.fill_sum += std::clamp(fillPct, 0.f, 100.f);
    ++b.fill_count;
}

void ServerMetrics::adjustActiveRequests(int delta) const {
    _activeRequests.fetch_add(delta, std::memory_order_relaxed);
}

void ServerMetrics::adjustActiveWebSockets(int delta) const {
    _activeWebSockets.fetch_add(delta, std::memory_order_relaxed);
}

int64_t ServerMetrics::getActiveRequests() const { return _activeRequests.load(std::memory_order_relaxed); }
int64_t ServerMetrics::getActiveWebSockets() const { return _activeWebSockets.load(std::memory_order_relaxed); }

ServerMetrics::Window ServerMetrics::getWindow(uint32_t minutes) const {
    const uint32_t curM = nowMinute_();
    Window w;
    double fillSum = 0.0;
    uint64_t fillN = 0;
    std::array<uint64_t, kLatBins> lat{};
    uint64_t latN = 0;
    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        for (const MinuteBucket& b : _minBuckets) {
            if (b.epoch == 0 || b.epoch > curM || curM - b.epoch >= minutes) {
                continue;
            }
            w.requests += b.requests;
            w.errors_4xx += b.errors_4xx;
            w.errors_5xx += b.errors_5xx;
            w.errors_int += b.errors_int;
            w.rejections += b.rejections;
            fillSum += b.fill_sum;
            fillN += b.fill_count;
            for (size_t i = 0; i < kLatBins; ++i) {
                lat[i] += b.lat[i];
                latN += b.lat[i];
            }
        }
    }
    if (fillN > 0) {
        w.avg_queue_fill = fillSum / static_cast<double>(fillN);
    }
    w.p50 = histogramPercentileMs(lat, latN, 0.50);
    w.p95 = histogramPercentileMs(lat, latN, 0.95);
    w.p99 = histogramPercentileMs(lat, latN, 0.99);
    return w;
}

uint64_t ServerMetrics::getUptimeSeconds() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _startTime).count());
}

double ServerMetrics::getUptimeHoursTotal() const {
    return static_cast<double>(_lifetimeUptimeBaselineSeconds + getUptimeSeconds()) / 3600.0;
}

void ServerMetrics::clearAllMetrics_() {
    for (auto& c : _counters) {
        c.store(0, std::memory_order_relaxed);
    }
    _activeRequests.store(0, std::memory_order_relaxed);
    _activeWebSockets.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _minBuckets.fill({});
    }
    std::lock_guard<std::mutex> lock(_errorMutex);
    _errors.clear();
    _errorSeq = 0;
}

bool ServerMetrics::importPersistentMetricsJson_(JSONParser root) {
    const int schema = root.hasKey("schema_version") ? root.getInt("schema_version") : 0;
    if (schema != 1 && schema != kMetricsSchemaVersion) {
        return false;
    }

    const std::vector<JSONParser> bucketObjs = root.getArrayOfJSON("buckets");
    if (bucketObjs.size() != _minBuckets.size()) {
        return false;
    }

    std::array<MinuteBucket, 60> buckets{};
    for (size_t i = 0; i < buckets.size(); ++i) {
        JSONParser b = bucketObjs[i];
        MinuteBucket& d = buckets[i];
        d.epoch = static_cast<uint32_t>(parseJsonU64(b, "epoch", 0));
        d.requests = static_cast<uint32_t>(parseJsonU64(b, "requests", 0));
        d.errors_4xx = static_cast<uint32_t>(parseJsonU64(b, "errors_4xx", 0));
        d.errors_5xx = static_cast<uint32_t>(parseJsonU64(b, "errors_5xx", 0));
        d.errors_int = static_cast<uint32_t>(parseJsonU64(b, "errors_int", 0));
        d.rejections = static_cast<uint32_t>(parseJsonU64(b, "rejections", 0));
        d.fill_sum = b.hasKey("fill_sum") ? b.getFloat("fill_sum") : 0.f;
        d.fill_count = static_cast<uint32_t>(parseJsonU64(b, "fill_count", 0));
        if (b.hasKey("lat")) {
            const std::vector<int> lat = b.getIntArray("lat");
            if (lat.size() == kLatBins) {
                for (size_t k = 0; k < kLatBins; ++k) {
                    d.lat[k] = lat[k] < 0 ? 0u : static_cast<uint32_t>(lat[k]);
                }
            }
        }
    }

    for (size_t i = 0; i < kCounterCount; ++i) {
        _counters[i].store(parseJsonU64(root, kCounterKeys[i], 0), std::memory_order_relaxed);
    }
    _activeRequests.store(0, std::memory_order_relaxed);
    _activeWebSockets.store(0, std::memory_order_relaxed);
    _lifetimeUptimeBaselineSeconds = parseJsonU64(root, "lifetime_uptime_seconds", 0);

    std::deque<ErrorEntry> errors;
    if (root.hasKey("error_log")) {
        for (JSONParser& j : root.getArrayOfJSON("error_log")) {
            errors.push_back(errorEntryFromJson(j));
        }
        const std::size_t capacity = getErrorLogCapacity();
        while (errors.size() > capacity) {
            errors.pop_front();
        }
    }
    const uint64_t errorSeq = std::max(parseJsonU64(root, "error_seq", 0), errors.empty() ? 0 : errors.back().seq);

    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _minBuckets = buckets;
    }
    std::lock_guard<std::mutex> lock(_errorMutex);
    _errors = std::move(errors);
    _errorSeq = errorSeq;
    return true;
}

bool ServerMetrics::loadPersistentMetricsFromFile(const std::string& path) {
    std::unique_ptr<JSONParser> parsed = getJSONFromFileSafe(path);
    bool ok = true;
    if (!parsed) {
        _lifetimeUptimeBaselineSeconds = 0;
    } else if (!importPersistentMetricsJson_(std::move(*parsed))) {
        clearAllMetrics_();
        _lifetimeUptimeBaselineSeconds = 0;
        ok = false;
    }
    _startTime = std::chrono::steady_clock::now();
    return ok;
}

bool ServerMetrics::savePersistentMetricsToFile(const std::string& path) const {
    std::array<MinuteBucket, 60> bucketsCopy{};
    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        bucketsCopy = _minBuckets;
    }

    JSONParser root;
    root.setInt("schema_version", kMetricsSchemaVersion);
    appendIsoUtcNow(root);
    root.setString("lifetime_uptime_seconds", std::to_string(_lifetimeUptimeBaselineSeconds + getUptimeSeconds()));
    for (size_t i = 0; i < kCounterCount; ++i) {
        root.setLongLong(kCounterKeys[i], static_cast<long long>(_counters[i].load(std::memory_order_relaxed)));
    }

    std::vector<JSONParser> bucketJson;
    bucketJson.reserve(bucketsCopy.size());
    for (const MinuteBucket& b : bucketsCopy) {
        JSONParser o;
        o.setLongLong("epoch", b.epoch);
        o.setLongLong("requests", b.requests);
        o.setLongLong("errors_4xx", b.errors_4xx);
        o.setLongLong("errors_5xx", b.errors_5xx);
        o.setLongLong("errors_int", b.errors_int);
        o.setLongLong("rejections", b.rejections);
        o.setDouble("fill_sum", static_cast<double>(b.fill_sum));
        o.setLongLong("fill_count", b.fill_count);
        o.setIntArray("lat", std::vector<int>(b.lat.begin(), b.lat.end()));
        bucketJson.push_back(std::move(o));
    }
    root.setArrayOfJSON("buckets", bucketJson);

    std::vector<JSONParser> errorJson;
    {
        std::lock_guard<std::mutex> lock(_errorMutex);
        root.setLongLong("error_seq", static_cast<long long>(_errorSeq));
        errorJson.reserve(_errors.size());
        for (const ErrorEntry& e : _errors) {
            errorJson.push_back(errorEntryJson(e));
        }
    }
    root.setArrayOfJSON("error_log", errorJson);

    return saveJSONToFileAtomic(root, path);
}

}  // namespace geruest
