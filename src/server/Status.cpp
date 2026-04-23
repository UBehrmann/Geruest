/**
 * @file server/Status.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief System metrics helpers and /status endpoint (enableStatus).
 */

#include "../Geruest.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/statvfs.h>

#include "geruest/Version.hpp"

namespace geruest {

// ---------------------------------------------------------------------------
// System metrics helpers
// ---------------------------------------------------------------------------

struct SysMemInfo {
    uint64_t total_mb     = 0;
    uint64_t used_mb      = 0;
    uint64_t free_mb      = 0;
    double   percent_used = 0.0;
};

struct SysCgroupMemInfo {
    bool     available    = false;
    uint64_t limit_mb     = 0;
    uint64_t used_mb      = 0;
    uint64_t free_mb      = 0;
    double   percent_used = 0.0;
};

struct SysCgroupCpuInfo {
    bool   available        = false;
    double allocated_cores  = 0.0;   // CPU quota / period (e.g. 2.0 = 2 CPUs)
    double usage_percent    = 0.0;   // % of allocated CPU used since last sample
};

struct SysCpuInfo {
    unsigned int cpu_count = 0;
    double load_1m  = 0.0;
    double load_5m  = 0.0;
    double load_15m = 0.0;
};

struct SysDiskInfo {
    uint64_t total_gb     = 0;
    uint64_t used_gb      = 0;
    uint64_t free_gb      = 0;
    double   percent_used = 0.0;
};

static SysMemInfo collectMemoryInfo() {
    SysMemInfo info;
    std::ifstream f("/proc/meminfo");
    std::string line;
    uint64_t total_kb = 0, avail_kb = 0;
    while (std::getline(f, line)) {
        if (line.compare(0, 9, "MemTotal:") == 0) {
            std::istringstream ss(line.substr(9)); ss >> total_kb;
        } else if (line.compare(0, 13, "MemAvailable:") == 0) {
            std::istringstream ss(line.substr(13)); ss >> avail_kb;
        }
    }
    info.total_mb     = total_kb / 1024;
    info.free_mb      = avail_kb / 1024;
    info.used_mb      = info.total_mb - info.free_mb;
    info.percent_used = info.total_mb > 0 ? 100.0 * static_cast<double>(info.used_mb) / static_cast<double>(info.total_mb) : 0.0;
    return info;
}

static SysCgroupMemInfo collectCgroupMemInfo() {
    SysCgroupMemInfo info;
    // Try cgroup v2 first
    {
        std::ifstream usage_f("/sys/fs/cgroup/memory.current");
        std::ifstream limit_f("/sys/fs/cgroup/memory.max");
        if (usage_f && limit_f) {
            std::string limit_str;
            uint64_t usage_bytes = 0;
            usage_f >> usage_bytes;
            limit_f >> limit_str;
            if (limit_str != "max" && !limit_str.empty()) {
                const uint64_t limit_bytes = std::stoull(limit_str);
                info.available    = true;
                info.limit_mb     = limit_bytes / (1024ULL * 1024);
                info.used_mb      = usage_bytes / (1024ULL * 1024);
                info.free_mb      = info.limit_mb > info.used_mb ? info.limit_mb - info.used_mb : 0;
                info.percent_used = info.limit_mb > 0 ? 100.0 * static_cast<double>(info.used_mb) / static_cast<double>(info.limit_mb) : 0.0;
                return info;
            }
        }
    }
    // Fall back to cgroup v1
    {
        std::ifstream usage_f("/sys/fs/cgroup/memory/memory.usage_in_bytes");
        std::ifstream limit_f("/sys/fs/cgroup/memory/memory.limit_in_bytes");
        if (usage_f && limit_f) {
            uint64_t usage_bytes = 0, limit_bytes = 0;
            usage_f >> usage_bytes;
            limit_f >> limit_bytes;
            if (limit_bytes < (1ULL << 62)) {
                info.available    = true;
                info.limit_mb     = limit_bytes / (1024ULL * 1024);
                info.used_mb      = usage_bytes / (1024ULL * 1024);
                info.free_mb      = info.limit_mb > info.used_mb ? info.limit_mb - info.used_mb : 0;
                info.percent_used = info.limit_mb > 0 ? 100.0 * static_cast<double>(info.used_mb) / static_cast<double>(info.limit_mb) : 0.0;
            }
        }
    }
    return info;
}

static SysCgroupCpuInfo collectCgroupCpuInfo() {
    SysCgroupCpuInfo info;
    double allocated = 0.0;
    bool   hasLimit  = false;

    // Try cgroup v2: cpu.max → "quota period" or "max period"
    {
        std::ifstream f("/sys/fs/cgroup/cpu.max");
        if (f) {
            std::string quota_str, period_str;
            f >> quota_str >> period_str;
            if (quota_str != "max" && !quota_str.empty() && !period_str.empty()) {
                const double period = std::stod(period_str);
                if (period > 0) { allocated = std::stod(quota_str) / period; hasLimit = true; }
            }
        }
    }
    // Fall back to cgroup v1
    if (!hasLimit) {
        std::ifstream qf("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
        std::ifstream pf("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
        if (qf && pf) {
            int64_t quota = 0, period = 0;
            qf >> quota; pf >> period;
            if (quota > 0 && period > 0) { allocated = static_cast<double>(quota) / static_cast<double>(period); hasLimit = true; }
        }
    }

    if (!hasLimit) return info;
    info.available       = true;
    info.allocated_cores = allocated;

    // Read cumulative CPU time (µs)
    uint64_t usage_usec = 0;
    bool     gotUsage   = false;

    // cgroup v2: cpu.stat → "usage_usec <n>"
    {
        std::ifstream f("/sys/fs/cgroup/cpu.stat");
        if (f) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.compare(0, 11, "usage_usec ") == 0) {
                    std::istringstream ss(line.substr(11));
                    ss >> usage_usec;
                    gotUsage = true;
                    break;
                }
            }
        }
    }
    // cgroup v1: cpuacct.usage (nanoseconds)
    if (!gotUsage) {
        std::ifstream f("/sys/fs/cgroup/cpuacct/cpuacct.usage");
        if (f) { uint64_t ns = 0; f >> ns; usage_usec = ns / 1000; gotUsage = true; }
    }

    if (gotUsage) {
        // Delta between consecutive calls — no blocking sleep needed
        static uint64_t                            prev_usec = 0;
        static std::chrono::steady_clock::time_point prev_tp = std::chrono::steady_clock::now();

        const auto   now        = std::chrono::steady_clock::now();
        const double elapsed_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(now - prev_tp).count());

        if (elapsed_us > 0 && prev_usec > 0 && usage_usec >= prev_usec) {
            const double cpu_delta = static_cast<double>(usage_usec - prev_usec);
            info.usage_percent = (cpu_delta / (elapsed_us * allocated)) * 100.0;
            if (info.usage_percent > 100.0) info.usage_percent = 100.0;
            if (info.usage_percent <   0.0) info.usage_percent =   0.0;
        }

        prev_usec = usage_usec;
        prev_tp   = now;
    }
    return info;
}

static SysCpuInfo collectCpuInfo() {
    SysCpuInfo info;
    info.cpu_count = std::thread::hardware_concurrency();
    std::ifstream f("/proc/loadavg");
    if (f) f >> info.load_1m >> info.load_5m >> info.load_15m;
    return info;
}

static SysDiskInfo collectDiskInfo() {
    SysDiskInfo info;
    struct statvfs sv{};
    if (statvfs("/", &sv) == 0) {
        const uint64_t block = sv.f_frsize;
        const uint64_t total = sv.f_blocks * block;
        const uint64_t free  = sv.f_bfree  * block;
        const uint64_t avail = sv.f_bavail * block;
        info.total_gb     = total / (1024ULL * 1024 * 1024);
        info.free_gb      = avail / (1024ULL * 1024 * 1024);
        info.used_gb      = (total - free) / (1024ULL * 1024 * 1024);
        info.percent_used = total > 0 ? 100.0 * static_cast<double>(total - free) / static_cast<double>(total) : 0.0;
    }
    return info;
}

// ---------------------------------------------------------------------------

void Geruest::enableStatus(const std::string& token) {
    _statusToken  = token;
    _statusActive = true;

    serverData.addRoute("/status", [this, token](const HTTPRequest& req) -> HTTPResponse {
        if (!serverData.isDevMode()) {
            const std::string authHeader  = req.getHeader("authorization");
            const std::string queryToken  = req.getParam("token");
            const std::string expected    = "Bearer " + token;
            const bool validHeader = (authHeader == expected);
            const bool validQuery  = (!queryToken.empty() && queryToken == token);
            if (!validHeader && !validQuery) {
                HTTPResponse resp("401 Unauthorized");
                resp.setHeader("WWW-Authenticate", "Bearer realm=\"status\"");
                resp.setHeader("Content-Type", "application/json");
                resp.setBody(R"({"error":"Unauthorized"})");
                return resp;
            }
        }

        // Collect system metrics
        const SysMemInfo       sysMem       = collectMemoryInfo();
        const SysCgroupMemInfo sysCgroupMem = collectCgroupMemInfo();
        const SysCgroupCpuInfo sysCgroupCpu = collectCgroupCpuInfo();
        const SysCpuInfo       sysCpu       = collectCpuInfo();
        const SysDiskInfo      sysDisk      = collectDiskInfo();

        // Collect server metrics
        const uint64_t uptime   = serverData.getUptimeSeconds();
        const ServerData::WindowMetrics wmHour  = serverData.getWindowMetricsHour();
        const ServerData::WindowMetrics avgHour = serverData.getRollingAveragePerHour();
        const uint64_t totalR   = serverData.getTotalRequests();
        const uint64_t totalE   = serverData.getTotalErrors();
        const uint64_t total4xx = serverData.getTotal4xx();
        const uint64_t total5xx = serverData.getTotal5xx();
        const uint64_t totalInt = serverData.getTotalInternalErrors();
        const uint64_t rejTotal = serverData.getQueueRejections();
        const uint64_t acceptErrTotal = serverData.getAcceptErrorsTotal();
        const uint64_t acceptEmfileTotal = serverData.getAcceptEmfileTotal();
        const uint64_t fileOpenFailures = serverData.getFileOpenFailures();
        const uint64_t overloadHttpResponses = serverData.getOverloadHttpResponses();
        const int64_t  active   = serverData.getActiveHandlers();
        const ServerData::LatencyStats lat = serverData.getLatencyStats(60);
        const uint64_t curQueue = static_cast<uint64_t>(_activeSessions.load(std::memory_order_relaxed));

        std::string health = "ok";
        if (wmHour.avg_queue_fill >= 80.0 || wmHour.requests >= 1000) {
            health = "overloaded";
        } else if (wmHour.avg_queue_fill >= 50.0 || wmHour.requests >= 500) {
            health = "degraded";
        }

        // Build ISO 8601 UTC timestamp
        std::time_t now_t = std::time(nullptr);
        char timeBuf[32]  = {};
        struct tm utcTm{};
        gmtime_r(&now_t, &utcTm);
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);

        // Build JSON objects
        JSONParser requests;
        requests.setLongLong("total",        static_cast<long long>(totalR));
        requests.setLongLong("active",       static_cast<long long>(active));
        requests.setLongLong("last_hour",    static_cast<long long>(wmHour.requests));
        requests.setLongLong("avg_per_hour", static_cast<long long>(avgHour.requests));

        JSONParser errors;
        errors.setLongLong("total",            static_cast<long long>(totalE));
        errors.setLongLong("client_4xx",       static_cast<long long>(total4xx));
        errors.setLongLong("server_5xx",       static_cast<long long>(total5xx));
        errors.setLongLong("internal",         static_cast<long long>(totalInt));
        errors.setLongLong("last_hour_4xx",    static_cast<long long>(wmHour.errors_4xx));
        errors.setLongLong("last_hour_5xx",    static_cast<long long>(wmHour.errors_5xx));
        errors.setLongLong("last_hour_int",    static_cast<long long>(wmHour.errors_int));
        errors.setLongLong("avg_per_hour_4xx", static_cast<long long>(avgHour.errors_4xx));
        errors.setLongLong("avg_per_hour_5xx", static_cast<long long>(avgHour.errors_5xx));
        errors.setLongLong("avg_per_hour_int", static_cast<long long>(avgHour.errors_int));

        JSONParser queue;
        queue.setLongLong("current_size",             static_cast<long long>(curQueue));
        queue.setLongLong("max_size",                 static_cast<long long>(_maxQueueSize));
        queue.setLongLong("rejections_total",         static_cast<long long>(rejTotal));
        queue.setLongLong("overload_http_responses",  static_cast<long long>(overloadHttpResponses));
        queue.setDouble("avg_fill_percent_hour",      wmHour.avg_queue_fill);
        queue.setDouble("avg_fill_percent_per_hour",  avgHour.avg_queue_fill);

        JSONParser io;
        io.setLongLong("accept_errors_total", static_cast<long long>(acceptErrTotal));
        io.setLongLong("accept_emfile_total", static_cast<long long>(acceptEmfileTotal));
        io.setLongLong("file_open_failures_total", static_cast<long long>(fileOpenFailures));

        JSONParser latency;
        latency.setDouble("p50", lat.p50);
        latency.setDouble("p95", lat.p95);
        latency.setDouble("p99", lat.p99);

        JSONParser memory;
        memory.setLongLong("total_mb",   static_cast<long long>(sysMem.total_mb));
        memory.setLongLong("used_mb",    static_cast<long long>(sysMem.used_mb));
        memory.setLongLong("free_mb",    static_cast<long long>(sysMem.free_mb));
        memory.setDouble("percent_used", sysMem.percent_used);

        JSONParser cpu;
        cpu.setInt("count",       static_cast<int>(sysCpu.cpu_count));
        cpu.setDouble("load_1m",  sysCpu.load_1m);
        cpu.setDouble("load_5m",  sysCpu.load_5m);
        cpu.setDouble("load_15m", sysCpu.load_15m);

        JSONParser disk;
        disk.setLongLong("total_gb",   static_cast<long long>(sysDisk.total_gb));
        disk.setLongLong("used_gb",    static_cast<long long>(sysDisk.used_gb));
        disk.setLongLong("free_gb",    static_cast<long long>(sysDisk.free_gb));
        disk.setDouble("percent_used", sysDisk.percent_used);

        JSONParser system;
        system.setJSON("memory", memory);
        system.setJSON("cpu",    cpu);
        system.setJSON("disk",   disk);

        if (sysCgroupMem.available) {
            JSONParser cgMem;
            cgMem.setLongLong("limit_mb",   static_cast<long long>(sysCgroupMem.limit_mb));
            cgMem.setLongLong("used_mb",    static_cast<long long>(sysCgroupMem.used_mb));
            cgMem.setLongLong("free_mb",    static_cast<long long>(sysCgroupMem.free_mb));
            cgMem.setDouble("percent_used", sysCgroupMem.percent_used);
            system.setJSON("cgroup_memory", cgMem);
        }

        if (sysCgroupCpu.available) {
            JSONParser cgCpu;
            cgCpu.setDouble("allocated_cores", sysCgroupCpu.allocated_cores);
            cgCpu.setDouble("usage_percent",   sysCgroupCpu.usage_percent);
            system.setJSON("cgroup_cpu", cgCpu);
        }

        JSONParser root;
        root.setString("health",           health);
        root.setString("version",          getVersion());
        root.setString("timestamp",        timeBuf);
        root.setLongLong("uptime_seconds", static_cast<long long>(uptime));
        root.setDouble("uptime_hours_total", serverData.getUptimeHoursTotal());
        root.setJSON("requests",           requests);
        root.setJSON("errors",             errors);
        root.setJSON("queue",              queue);
        root.setJSON("io",                 io);
        root.setJSON("latency_ms",         latency);
        root.setJSON("system",             system);

        HTTPResponse resp("200 OK");
        resp.setHeader("Content-Type",  "application/json");
        resp.setHeader("Cache-Control", "no-store");
        resp.setBody(root.toString());
        return resp;
    });

    sendToLogger("Status endpoint activated at /status (token-protected)");
}

}  // namespace geruest
