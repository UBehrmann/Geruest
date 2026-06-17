/**
 * @file ServerData.cpp
 * @brief Persistent metrics snapshot (JSON) for /status-related counters and windows.
 */

#include "ServerData.hpp"
#include "builders/AssetMerger.hpp"
#include "parser/JSONParser.hpp"

#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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

void ServerData::clearAllMetrics_() {
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
    _latHead  = 0;
    _latCount = 0;
    _latSamples.fill({});
}

bool ServerData::importPersistentMetricsJson_(JSONParser root) {
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
    const std::vector<JSONParser> latSamples = latRoot.hasKey("samples")
        ? latRoot.getArrayOfJSON("samples")
        : std::vector<JSONParser>{};
    const uint64_t latCount = latRoot.hasKey("count")
        ? parseJsonU64(latRoot, "count", 0)
        : 0;
    if (latCount > _LAT_CAP) {
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
        buckets[i].epoch      = static_cast<uint32_t>(parseJsonU64(b, "epoch", 0));
        buckets[i].requests   = static_cast<uint32_t>(parseJsonU64(b, "requests", 0));
        buckets[i].errors_4xx = static_cast<uint32_t>(parseJsonU64(b, "errors_4xx", 0));
        buckets[i].errors_5xx = static_cast<uint32_t>(parseJsonU64(b, "errors_5xx", 0));
        buckets[i].errors_int = static_cast<uint32_t>(parseJsonU64(b, "errors_int", 0));
        buckets[i].fill_sum   = b.hasKey("fill_sum") ? b.getFloat("fill_sum") : 0.f;
        buckets[i].fill_count = static_cast<uint32_t>(parseJsonU64(b, "fill_count", 0));
    }

    std::array<LatencySample, _LAT_CAP> latBuf{};
    for (uint64_t i = 0; i < latCount; ++i) {
        JSONParser s = latSamples[static_cast<size_t>(i)];
        latBuf[static_cast<size_t>(i)].epoch_s = static_cast<uint32_t>(parseJsonU64(s, "epoch_s", 0));
        latBuf[static_cast<size_t>(i)].us      = static_cast<uint32_t>(parseJsonU64(s, "us", 0));
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
        _latCount   = static_cast<size_t>(latCount);
        _latSamples = latBuf;
        // Samples stored oldest-first in [0 .. count); ring head follows last slot (matches getLatencyStats).
        _latHead = latCount > 0 ? (static_cast<size_t>(latCount) % _LAT_CAP) : 0;
    }

    return true;
}

bool ServerData::loadPersistentMetricsFromFile(const std::string& path) {
    const auto resetSessionStart = [this] {
        _startTime = std::chrono::steady_clock::now();
    };

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

bool ServerData::savePersistentMetricsToFile(const std::string& path) const {
    std::array<RollingBucket, 60> bucketsCopy{};
    std::array<LatencySample, _LAT_CAP> latCopy{};
    size_t latHeadCopy = 0;
    size_t latCountCopy = 0;
    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        bucketsCopy  = _minBuckets;
        latCopy      = _latSamples;
        latHeadCopy  = _latHead;
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
        const size_t start = (latHeadCopy + _LAT_CAP - latCountCopy) % _LAT_CAP;
        for (size_t i = 0; i < latCountCopy; ++i) {
            const LatencySample& s = latCopy[(start + i) % _LAT_CAP];
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

std::optional<std::string> ServerData::findMergedAssetOwnerPagePath(const std::string& assetRequestPath) const {
    if (!_mergeAssets || _root.empty()) {
        return std::nullopt;
    }

    const std::string canon = canonicalRequestPath(assetRequestPath);
    const bool isJs = canon.size() > 3 && canon.compare(canon.size() - 3, 3, ".js") == 0;
    const bool isCss = canon.size() > 4 && canon.compare(canon.size() - 4, 4, ".css") == 0;
    if (!isJs && !isCss) {
        return std::nullopt;
    }

    const size_t dotPos = canon.find_last_of('.');
    if (dotPos == std::string::npos || dotPos <= 1) {
        return std::nullopt;
    }

    const size_t lastSlash = canon.find_last_of('/');
    const std::string pageStem =
        (lastSlash == std::string::npos) ? canon.substr(1, dotPos - 1)
                                         : canon.substr(lastSlash + 1, dotPos - lastSlash - 1);
    if (pageStem.empty()) {
        return std::nullopt;
    }

    const std::string htmlRoot = _root + "/html";
    if (!std::filesystem::is_directory(htmlRoot)) {
        return std::nullopt;
    }

    const std::string templatePath = AssetMerger::findHtmlTemplateByPageName(htmlRoot, pageStem);
    if (templatePath.empty()) {
        return std::nullopt;
    }

    std::ifstream in(templatePath, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::string htmlContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (htmlContent.empty()) {
        return std::nullopt;
    }

    AssetMerger merger(_root, _removeComments, _obfuscationExclusions);
    const std::string pageName = AssetMerger::pageNameFromHtmlPath(templatePath);
    const std::vector<std::string> predicted = merger.predictMergedAssetUrls(htmlContent, pageName);
    for (const std::string& url : predicted) {
        if (canonicalRequestPath(url) == canon) {
            return AssetMerger::sitePathFromHtmlFile(htmlRoot, templatePath);
        }
    }
    return std::nullopt;
}

}  // namespace geruest
