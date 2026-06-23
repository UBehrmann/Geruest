/**
 * @file StatusEndpoint_tests.cpp
 * @brief Regression: sync routes with large stack frames (notably GET /status) must not
 *        run inside a nested coroutine frame — that overflowed the stack (exit 139).
 */

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "Geruest.hpp"
#include "parser/JSONParser.hpp"

using namespace geruest;

namespace {

constexpr const char* kStatusToken = "test-status-token";

bool setSocketTimeouts(int fd, int timeoutMs) {
    timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000) * 1000);
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

void wakeAcceptOnLocalhost(int port) {
    if (port <= 0 || port > 65535) {
        return;
    }
    const int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) {
        return;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    (void)::connect(c, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    close(c);
}

int connectTo(int port, int timeoutMs = 5000) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (!setSocketTimeouts(fd, timeoutMs)) {
        close(fd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return -1;
    }

    const int connectRc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (connectRc != 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    if (connectRc != 0) {
        pollfd pfd{fd, POLLOUT, 0};
        if (poll(&pfd, 1, timeoutMs) <= 0) {
            close(fd);
            return -1;
        }
        int sockErr = 0;
        socklen_t errLen = sizeof(sockErr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockErr, &errLen) != 0 || sockErr != 0) {
            close(fd);
            return -1;
        }
    }

    if (fcntl(fd, F_SETFL, flags) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool sendAll(int fd, const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
        const ssize_t n = ::send(fd, p + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recvSome(int fd, std::string& out, size_t minBytes, int timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (out.size() < minBytes && std::chrono::steady_clock::now() < deadline) {
        char buf[8192];
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            out.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }
        pollfd pfd{fd, POLLIN, 0};
        const int remain = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())
                .count());
        if (remain <= 0) {
            break;
        }
        (void)poll(&pfd, 1, remain);
    }
    return out.size() >= minBytes;
}

HTTPResponse buildHeavySyncJsonResponse() {
    JSONParser root;
    root.setString("kind", "heavy-sync-regression");

    JSONParser requests;
    requests.setLongLong("total", 42);
    requests.setLongLong("active", 3);
    requests.setLongLong("last_hour", 7);
    requests.setLongLong("avg_per_hour", 1);

    JSONParser errors;
    errors.setLongLong("total", 0);
    errors.setLongLong("client_4xx", 0);
    errors.setLongLong("server_5xx", 0);
    errors.setLongLong("internal", 0);

    JSONParser queue;
    queue.setLongLong("current_size", 1);
    queue.setLongLong("max_size", 500);
    queue.setDouble("avg_fill_percent_hour", 12.5);

    JSONParser memory;
    memory.setLongLong("total_mb", 1024);
    memory.setLongLong("used_mb", 512);
    memory.setDouble("percent_used", 50.0);

    JSONParser cpu;
    cpu.setInt("count", 4);
    cpu.setDouble("load_1m", 0.42);
    cpu.setDouble("load_5m", 0.35);
    cpu.setDouble("load_15m", 0.30);

    JSONParser disk;
    disk.setLongLong("total_gb", 100);
    disk.setLongLong("used_gb", 40);
    disk.setDouble("percent_used", 40.0);

    JSONParser system;
    system.setJSON("memory", memory);
    system.setJSON("cpu", cpu);
    system.setJSON("disk", disk);

    JSONParser cgroupMem;
    cgroupMem.setLongLong("limit_mb", 2048);
    cgroupMem.setLongLong("used_mb", 256);
    cgroupMem.setDouble("percent_used", 12.5);
    system.setJSON("cgroup_memory", cgroupMem);

    JSONParser cgroupCpu;
    cgroupCpu.setDouble("allocated_cores", 2.0);
    cgroupCpu.setDouble("usage_percent", 3.5);
    system.setJSON("cgroup_cpu", cgroupCpu);

    root.setJSON("requests", requests);
    root.setJSON("errors", errors);
    root.setJSON("queue", queue);
    root.setJSON("system", system);

    HTTPResponse resp("200 OK");
    resp.setHeader("Content-Type", "application/json");
    resp.setBody(root.toString());
    return resp;
}

class ScopedBackgroundServer {
public:
    ~ScopedBackgroundServer() { shutdown(); }

    void launch(Geruest& server) {
        shutdown();

        const auto tag = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        metricsDir_ = std::filesystem::temp_directory_path() / ("geruest_status_int_" + tag);
        std::filesystem::create_directories(metricsDir_);

        server_ = &server;
        server.setPort(0);
        server.setWorkerThreadCount(2);
        server.setMaxQueueSize(32);
        server.setStatusPersistencePath((metricsDir_ / "metrics.json").string());
        ASSERT_TRUE(server.init());
        listenPort_ = server.getListenPort();
        ASSERT_GT(listenPort_, 0);

        thread_ = std::thread([&server] { server.start(); });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!server.isRunning() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ASSERT_TRUE(server.isRunning()) << "background server failed to start";
    }

    int listenPort() const { return listenPort_; }

    bool running() const { return server_ != nullptr && server_->isRunning(); }

    void shutdown() {
        if (server_ != nullptr && server_->isRunning()) {
            server_->stop();
            wakeAcceptOnLocalhost(listenPort_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        server_     = nullptr;
        listenPort_ = -1;
        if (!metricsDir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(metricsDir_, ec);
            metricsDir_.clear();
        }
    }

private:
    Geruest*              server_{nullptr};
    std::thread           thread_;
    int                   listenPort_{-1};
    std::filesystem::path metricsDir_;
};

std::string httpGet(int port, std::string_view path, std::string_view extraHeaders = {}) {
    const int fd = connectTo(port);
    if (fd < 0) {
        return {};
    }

    std::string request = "GET ";
    request.append(path);
    request.append(
        " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n");
    if (!extraHeaders.empty()) {
        request.append(extraHeaders);
        if (extraHeaders.back() != '\n') {
            request.push_back('\n');
        }
    }
    request.append("\r\n");

    if (!sendAll(fd, request.data(), request.size())) {
        close(fd);
        return {};
    }

    std::string response;
    if (!recvSome(fd, response, 12)) {
        close(fd);
        return {};
    }
    close(fd);
    return response;
}

}  // namespace

TEST(StatusEndpoint, HeavySyncRouteSurvivesRepeatedRequests) {
    Geruest server;
    server.addRoute("/heavy-sync", [](const HTTPRequest&) { return buildHeavySyncJsonResponse(); });

    ScopedBackgroundServer bg;
    bg.launch(server);

    for (int i = 0; i < 20; ++i) {
        const std::string response = httpGet(bg.listenPort(), "/heavy-sync");
        ASSERT_FALSE(response.empty()) << "poll " << i;
        EXPECT_NE(response.find("200 OK"), std::string::npos) << "poll " << i;
        EXPECT_NE(response.find("heavy-sync-regression"), std::string::npos) << "poll " << i;
        ASSERT_TRUE(bg.running()) << "server died on poll " << i;
    }
}

TEST(StatusEndpoint, EnableStatusSurvivesRepeatedPolls) {
    Geruest server;
    server.enableStatus(kStatusToken);

    ScopedBackgroundServer bg;
    bg.launch(server);

    const std::string authHeader = std::string("Authorization: Bearer ") + kStatusToken + "\r\n";

    for (int i = 0; i < 20; ++i) {
        const std::string response = httpGet(bg.listenPort(), "/status", authHeader);
        ASSERT_FALSE(response.empty()) << "poll " << i;
        EXPECT_NE(response.find("200 OK"), std::string::npos) << "poll " << i;
        EXPECT_NE(response.find("\"health\""), std::string::npos) << "poll " << i;
        ASSERT_TRUE(bg.running()) << "server died on poll " << i;
    }
}

TEST(StatusEndpoint, EnableStatusRequiresToken) {
    Geruest server;
    server.enableStatus(kStatusToken);

    ScopedBackgroundServer bg;
    bg.launch(server);

    const std::string response = httpGet(bg.listenPort(), "/status");
    ASSERT_FALSE(response.empty());
    EXPECT_NE(response.find("401 Unauthorized"), std::string::npos);
    ASSERT_TRUE(bg.running());
}

TEST(StatusEndpoint, ConcurrentStatusPolls) {
    Geruest server;
    server.enableStatus(kStatusToken);

    ScopedBackgroundServer bg;
    bg.launch(server);

    const int port = bg.listenPort();
    const std::string authHeader = std::string("Authorization: Bearer ") + kStatusToken + "\r\n";

    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    workers.reserve(4);
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([port, &authHeader, &failures] {
            for (int i = 0; i < 5; ++i) {
                const std::string response = httpGet(port, "/status", authHeader);
                if (response.empty() || response.find("200 OK") == std::string::npos) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (std::thread& w : workers) {
        w.join();
    }

    EXPECT_EQ(failures.load(std::memory_order_relaxed), 0);
    ASSERT_TRUE(bg.running());
}
