/**
 * @file server/Status.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief System metrics helpers and /status endpoint (enableStatus). Fields: doc/STATUS.md.
 */

#include "../Geruest.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <vector>

#include "geruest/Version.hpp"

namespace geruest {

namespace {

using Counter = ServerMetrics::Counter;

/** Bump on breaking changes to the /status JSON shape (removed/renamed fields, changed semantics). */
constexpr int kStatusVersion = 2;

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
    double usage_percent    = 0.0;   // % of allocated CPU used over the last sample interval
};

struct SysCpuInfo {
    unsigned int cpu_count = 0;
    double load_1m  = 0.0;
    double load_5m  = 0.0;
    double load_15m = 0.0;
};

struct SysDiskInfo {
    std::string path;
    uint64_t total_gb     = 0;
    uint64_t used_gb      = 0;
    uint64_t free_gb      = 0;
    double   percent_used = 0.0;
};

struct ProcessInfo {
    uint64_t rss_mb   = 0;
    uint64_t threads  = 0;
    uint64_t open_fds = 0;
    uint64_t max_fds  = 0;  // 0 = unlimited / unknown
    double   fd_percent_used = 0.0;
};

std::mutex       g_cgroupCpuSampleMu;
uint64_t         g_cgroupCpuPrevUsec = 0;
std::chrono::steady_clock::time_point g_cgroupCpuPrevTp = std::chrono::steady_clock::now();
SysCgroupCpuInfo g_cgroupCpuLast;

SysMemInfo collectMemoryInfo() {
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

SysCgroupMemInfo collectCgroupMemInfo() {
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

/** Reads quota + cumulative usage and updates the shared sample; only the background sampler calls this. */
void sampleCgroupCpu() {
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

    if (!hasLimit) {
        std::lock_guard<std::mutex> lock(g_cgroupCpuSampleMu);
        g_cgroupCpuLast = info;
        return;
    }
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

    std::lock_guard<std::mutex> lock(g_cgroupCpuSampleMu);
    if (gotUsage) {
        const auto now = std::chrono::steady_clock::now();
        const double elapsed_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(now - g_cgroupCpuPrevTp).count());

        if (elapsed_us > 0 && g_cgroupCpuPrevUsec > 0 && usage_usec >= g_cgroupCpuPrevUsec) {
            const double cpu_delta = static_cast<double>(usage_usec - g_cgroupCpuPrevUsec);
            info.usage_percent = std::clamp((cpu_delta / (elapsed_us * allocated)) * 100.0, 0.0, 100.0);
        }

        g_cgroupCpuPrevUsec = usage_usec;
        g_cgroupCpuPrevTp   = now;
    }
    g_cgroupCpuLast = info;
}

SysCgroupCpuInfo lastCgroupCpuSample() {
    std::lock_guard<std::mutex> lock(g_cgroupCpuSampleMu);
    return g_cgroupCpuLast;
}

SysCpuInfo collectCpuInfo() {
    SysCpuInfo info;
    info.cpu_count = std::thread::hardware_concurrency();
    std::ifstream f("/proc/loadavg");
    if (f) f >> info.load_1m >> info.load_5m >> info.load_15m;
    return info;
}

SysDiskInfo collectDiskInfo(const std::string& preferredPath) {
    SysDiskInfo info;
    struct statvfs sv{};
    info.path = preferredPath.empty() ? "/" : preferredPath;
    if (statvfs(info.path.c_str(), &sv) != 0) {
        info.path = "/";
        if (statvfs("/", &sv) != 0) {
            return info;
        }
    }
    const uint64_t block = sv.f_frsize;
    const uint64_t total = sv.f_blocks * block;
    const uint64_t free  = sv.f_bfree  * block;
    const uint64_t avail = sv.f_bavail * block;
    info.total_gb     = total / (1024ULL * 1024 * 1024);
    info.free_gb      = avail / (1024ULL * 1024 * 1024);
    info.used_gb      = (total - free) / (1024ULL * 1024 * 1024);
    info.percent_used = total > 0 ? 100.0 * static_cast<double>(total - free) / static_cast<double>(total) : 0.0;
    return info;
}

ProcessInfo collectProcessInfo() {
    ProcessInfo info;
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            uint64_t kb = 0;
            std::istringstream(line.substr(6)) >> kb;
            info.rss_mb = kb / 1024;
        } else if (line.compare(0, 8, "Threads:") == 0) {
            std::istringstream(line.substr(8)) >> info.threads;
        }
    }

    std::error_code ec;
    for (std::filesystem::directory_iterator it("/proc/self/fd", ec), end; !ec && it != end; it.increment(ec)) {
        ++info.open_fds;
    }

    rlimit rl{};
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) {
        info.max_fds = static_cast<uint64_t>(rl.rlim_cur);
        info.fd_percent_used = info.max_fds > 0 ? 100.0 * static_cast<double>(info.open_fds) / static_cast<double>(info.max_fds) : 0.0;
    }
    return info;
}

bool constantTimeEquals(std::string_view a, std::string_view b) {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

struct Health {
    int level = 0;  // 0 ok, 1 degraded, 2 overloaded
    std::vector<std::string> reasons;

    void flag(int lvl, std::string why) {
        level = std::max(level, lvl);
        reasons.push_back(std::move(why));
    }
    const char* name() const { return level == 2 ? "overloaded" : level == 1 ? "degraded" : "ok"; }
};

std::string pct(double v) { return std::to_string(static_cast<long long>(v + 0.5)) + "%"; }

// ponytail: fixed thresholds; make them configurable if a deployment needs different limits.
Health evaluateHealth(const ServerMetrics::Window& w5, double memPercent, const ProcessInfo& proc,
                      const SysDiskInfo& disk) {
    Health h;
    if (w5.rejections > 0) {
        h.flag(2, std::to_string(w5.rejections) + " connections rejected at session cap (5m)");
    }
    if (w5.avg_queue_fill >= 80.0) {
        h.flag(2, "queue fill " + pct(w5.avg_queue_fill) + " (5m)");
    } else if (w5.avg_queue_fill >= 50.0) {
        h.flag(1, "queue fill " + pct(w5.avg_queue_fill) + " (5m)");
    }
    if (memPercent >= 95.0) {
        h.flag(2, "memory " + pct(memPercent));
    } else if (memPercent >= 90.0) {
        h.flag(1, "memory " + pct(memPercent));
    }
    if (proc.fd_percent_used >= 90.0) {
        h.flag(2, "file descriptors " + pct(proc.fd_percent_used));
    } else if (proc.fd_percent_used >= 80.0) {
        h.flag(1, "file descriptors " + pct(proc.fd_percent_used));
    }
    if (w5.requests >= 20) {
        const double errRate = 100.0 * static_cast<double>(w5.errors_5xx + w5.errors_int) / static_cast<double>(w5.requests);
        if (errRate >= 5.0) {
            h.flag(1, "5xx rate " + pct(errRate) + " (5m)");
        }
    }
    if (w5.p99 >= 2000.0) {
        h.flag(1, "p99 latency " + std::to_string(static_cast<long long>(w5.p99)) + " ms (5m)");
    }
    if (disk.percent_used >= 95.0) {
        h.flag(1, "disk " + pct(disk.percent_used));
    }
    return h;
}

JSONParser latencyJson(const ServerMetrics::Window& w) {
    JSONParser j;
    j.setDouble("p50", w.p50);
    j.setDouble("p95", w.p95);
    j.setDouble("p99", w.p99);
    return j;
}

JSONParser requestsJson(const ServerMetrics& m, const ServerMetrics::Window& w5, const ServerMetrics::Window& w60,
                        double lifetimeHours) {
    JSONParser j;
    j.setLongLong("total",        static_cast<long long>(m.get(Counter::Requests)));
    j.setLongLong("active",       static_cast<long long>(m.getActiveRequests()));
    j.setLongLong("last_5m",      static_cast<long long>(w5.requests));
    j.setLongLong("last_hour",    static_cast<long long>(w60.requests));
    j.setDouble("avg_per_hour",   static_cast<double>(m.get(Counter::Requests)) / lifetimeHours);
    j.setLongLong("status_2xx",   static_cast<long long>(m.get(Counter::Status2xx)));
    j.setLongLong("status_3xx",   static_cast<long long>(m.get(Counter::Status3xx)));
    return j;
}

JSONParser errorsJson(const ServerMetrics& m, const ServerMetrics::Window& w5, const ServerMetrics::Window& w60,
                      double lifetimeHours) {
    const uint64_t t4 = m.get(Counter::Status4xx);
    const uint64_t t5 = m.get(Counter::Status5xx);
    const uint64_t ti = m.get(Counter::InternalErrors);
    JSONParser j;
    j.setLongLong("total",            static_cast<long long>(t4 + t5 + ti));
    j.setLongLong("client_4xx",       static_cast<long long>(t4));
    j.setLongLong("server_5xx",       static_cast<long long>(t5));
    j.setLongLong("internal",         static_cast<long long>(ti));
    j.setLongLong("last_5m_4xx",      static_cast<long long>(w5.errors_4xx));
    j.setLongLong("last_5m_5xx",      static_cast<long long>(w5.errors_5xx));
    j.setLongLong("last_5m_int",      static_cast<long long>(w5.errors_int));
    j.setLongLong("last_hour_4xx",    static_cast<long long>(w60.errors_4xx));
    j.setLongLong("last_hour_5xx",    static_cast<long long>(w60.errors_5xx));
    j.setLongLong("last_hour_int",    static_cast<long long>(w60.errors_int));
    j.setDouble("avg_per_hour_4xx",   static_cast<double>(t4) / lifetimeHours);
    j.setDouble("avg_per_hour_5xx",   static_cast<double>(t5) / lifetimeHours);
    j.setDouble("avg_per_hour_int",   static_cast<double>(ti) / lifetimeHours);
    j.setDouble("server_error_rate_5m", w5.requests > 0
        ? 100.0 * static_cast<double>(w5.errors_5xx + w5.errors_int) / static_cast<double>(w5.requests) : 0.0);
    j.setLongLong("status_auth_failures", static_cast<long long>(m.get(Counter::StatusAuthFailures)));
    return j;
}

JSONParser processJson(const ProcessInfo& p) {
    JSONParser j;
    j.setLongLong("rss_mb",   static_cast<long long>(p.rss_mb));
    j.setLongLong("threads",  static_cast<long long>(p.threads));
    j.setLongLong("open_fds", static_cast<long long>(p.open_fds));
    j.setLongLong("max_fds",  static_cast<long long>(p.max_fds));
    j.setDouble("fd_percent_used", p.fd_percent_used);
    return j;
}

JSONParser systemJson(const SysMemInfo& mem, const SysCgroupMemInfo& cgMem, const SysDiskInfo& sysDisk) {
    const SysCpuInfo sysCpu = collectCpuInfo();
    const SysCgroupCpuInfo cgCpu = lastCgroupCpuSample();

    JSONParser memory;
    memory.setLongLong("total_mb",   static_cast<long long>(mem.total_mb));
    memory.setLongLong("used_mb",    static_cast<long long>(mem.used_mb));
    memory.setLongLong("free_mb",    static_cast<long long>(mem.free_mb));
    memory.setDouble("percent_used", mem.percent_used);

    JSONParser cpu;
    cpu.setInt("count",       static_cast<int>(sysCpu.cpu_count));
    cpu.setDouble("load_1m",  sysCpu.load_1m);
    cpu.setDouble("load_5m",  sysCpu.load_5m);
    cpu.setDouble("load_15m", sysCpu.load_15m);

    JSONParser disk;
    disk.setString("path",         sysDisk.path);
    disk.setLongLong("total_gb",   static_cast<long long>(sysDisk.total_gb));
    disk.setLongLong("used_gb",    static_cast<long long>(sysDisk.used_gb));
    disk.setLongLong("free_gb",    static_cast<long long>(sysDisk.free_gb));
    disk.setDouble("percent_used", sysDisk.percent_used);

    JSONParser system;
    system.setJSON("memory", memory);
    system.setJSON("cpu",    cpu);
    system.setJSON("disk",   disk);

    if (cgMem.available) {
        JSONParser j;
        j.setLongLong("limit_mb",   static_cast<long long>(cgMem.limit_mb));
        j.setLongLong("used_mb",    static_cast<long long>(cgMem.used_mb));
        j.setLongLong("free_mb",    static_cast<long long>(cgMem.free_mb));
        j.setDouble("percent_used", cgMem.percent_used);
        system.setJSON("cgroup_memory", j);
    }
    if (cgCpu.available) {
        JSONParser j;
        j.setDouble("allocated_cores", cgCpu.allocated_cores);
        j.setDouble("usage_percent",   cgCpu.usage_percent);
        system.setJSON("cgroup_cpu", j);
    }
    return system;
}

}  // namespace

// ---------------------------------------------------------------------------

void Geruest::sampleStatusGauges() {
    if (_maxQueueSize > 0) {
        serverData.metrics().recordQueueFill(
            100.0f * static_cast<float>(_activeSessions.load(std::memory_order_relaxed)) /
            static_cast<float>(_maxQueueSize));
    }
    static unsigned calls = 0;
    if (calls++ % 5 == 0) {
        sampleCgroupCpu();
    }
}

HTTPResponse Geruest::handleStatusRequest(const HTTPRequest& req) const {
    if (!_statusActive) {
        HTTPResponse resp("503 Service Unavailable");
        resp.setHeader("Content-Type", "application/json");
        resp.setBody(R"({"error":"Status endpoint not active"})");
        return resp;
    }

    const ServerMetrics& m = serverData.metrics();

    if (!serverData.isDevMode()) {
        const std::string expected   = "Bearer " + _statusToken;
        const std::string queryToken = req.getParam("token");
        const bool validHeader = constantTimeEquals(req.getHeader("authorization"), expected);
        const bool validQuery  = !queryToken.empty() && constantTimeEquals(queryToken, _statusToken);
        if (!validHeader && !validQuery) {
            m.add(Counter::StatusAuthFailures);
            HTTPResponse resp("401 Unauthorized");
            resp.setHeader("WWW-Authenticate", "Bearer realm=\"status\"");
            resp.setHeader("Content-Type", "application/json");
            resp.setBody(R"({"error":"Unauthorized"})");
            return resp;
        }
    }

    const ServerMetrics::Window w5  = m.getWindow(5);
    const ServerMetrics::Window w60 = m.getWindow(60);
    const double lifetimeHours = std::max(1.0, m.getUptimeHoursTotal());

    const SysMemInfo       sysMem  = collectMemoryInfo();
    const SysCgroupMemInfo cgMem   = collectCgroupMemInfo();
    const SysDiskInfo      sysDisk = collectDiskInfo(serverData.getRoot());
    const ProcessInfo      proc    = collectProcessInfo();
    const Health health = evaluateHealth(w5, cgMem.available ? cgMem.percent_used : sysMem.percent_used, proc, sysDisk);

    std::time_t now_t = std::time(nullptr);
    char timeBuf[32]  = {};
    struct tm utcTm{};
    gmtime_r(&now_t, &utcTm);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);

    JSONParser root;
    root.setInt("status_version",      kStatusVersion);
    root.setString("health",           health.name());
    root.setStringArray("health_reasons", health.reasons);
    root.setString("version",          getVersion());
    root.setString("timestamp",        timeBuf);
    root.setLongLong("uptime_seconds", static_cast<long long>(m.getUptimeSeconds()));
    root.setDouble("uptime_hours_total", m.getUptimeHoursTotal());
    root.setJSON("requests", requestsJson(m, w5, w60, lifetimeHours));
    root.setJSON("errors",   errorsJson(m, w5, w60, lifetimeHours));
    {
        JSONParser queue;
        queue.setLongLong("current_size",       static_cast<long long>(_activeSessions.load(std::memory_order_relaxed)));
        queue.setLongLong("max_size",           static_cast<long long>(_maxQueueSize));
        queue.setLongLong("rejections_total",   static_cast<long long>(m.get(Counter::QueueRejections)));
        queue.setLongLong("rejections_last_5m", static_cast<long long>(w5.rejections));
        queue.setDouble("avg_fill_percent_5m",   w5.avg_queue_fill);
        queue.setDouble("avg_fill_percent_hour", w60.avg_queue_fill);
        root.setJSON("queue", queue);
    }
    {
        JSONParser io;
        io.setLongLong("connections_total",        static_cast<long long>(m.get(Counter::Connections)));
        io.setLongLong("accept_errors_total",      static_cast<long long>(m.get(Counter::AcceptErrors)));
        io.setLongLong("accept_emfile_total",      static_cast<long long>(m.get(Counter::AcceptEmfile)));
        io.setLongLong("framing_rejects_total",    static_cast<long long>(m.get(Counter::FramingRejects)));
        io.setLongLong("write_failures_total",     static_cast<long long>(m.get(Counter::WriteFailures)));
        io.setLongLong("file_open_failures_total", static_cast<long long>(m.get(Counter::FileOpenFailures)));
        root.setJSON("io", io);
    }
    {
        JSONParser ws;
        ws.setLongLong("active",         static_cast<long long>(m.getActiveWebSockets()));
        ws.setLongLong("upgrades_total", static_cast<long long>(m.get(Counter::WebSocketUpgrades)));
        root.setJSON("websocket", ws);
    }
    root.setJSON("latency_ms",      latencyJson(w5));
    root.setJSON("latency_ms_hour", latencyJson(w60));
    root.setJSON("process",         processJson(proc));
#if GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
    if (const EmailSender* email = _emailSender.get()) {
        JSONParser j;
        j.setLongLong("queue_size",     static_cast<long long>(email->getQueueSize()));
        j.setLongLong("sent_total",     static_cast<long long>(email->getEmailsSent()));
        j.setLongLong("failed_total",   static_cast<long long>(email->getEmailsFailed()));
        j.setLongLong("rejected_total", static_cast<long long>(email->getEmailsRejected()));
        root.setJSON("email", j);
    }
#endif
    root.setJSON("system", systemJson(sysMem, cgMem, sysDisk));

    HTTPResponse resp("200 OK");
    resp.setHeader("Content-Type",  "application/json");
    resp.setHeader("Cache-Control", "no-store");
    resp.setBody(root.toString());
    return resp;
}

void Geruest::enableStatus(const std::string& token) {
    _statusToken  = token;
    _statusActive = true;

    serverData.addRoute("/status", [this](const HTTPRequest& req) -> HTTPResponse {
        return handleStatusRequest(req);
    });

    sendToLogger("Status endpoint activated at /status (token-protected)");
}

}  // namespace geruest
