/**
 * @file GeruestLifecycle_tests.cpp
 * @brief Smoke tests for server lifecycle: destructor joins status-persistence thread,
 *        and start/stop exits without deadlock.
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "Geruest.hpp"

using namespace geruest;

namespace {

void wakeAcceptOnLocalhost(int port) {
    if (port <= 0 || port > 65535) {
        return;
    }
#ifdef _WIN32
    SOCKET c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c == INVALID_SOCKET) {
        return;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    (void)connect(c, reinterpret_cast<sockaddr*>(&a), static_cast<int>(sizeof(a)));
    closesocket(c);
#else
    const int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) {
        return;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    (void)::connect(c, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    close(c);
#endif
}

}  // namespace

TEST(GeruestLifecycle, DestructWithoutStartCompletes) {
    Geruest server;
    (void)server;
}

TEST(GeruestLifecycle, InitStartStopJoinsCleanly) {
    namespace fs = std::filesystem;
    const std::string tag =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path tmpDir = fs::temp_directory_path() / ("geruest_lifecycle_" + tag);
    fs::remove_all(tmpDir);
    fs::create_directories(tmpDir);
    const std::string metricsPath = (tmpDir / "metrics.json").string();

    {
        Geruest server;
        server.setPort(0);
        server.setWorkerThreadCount(1);
        server.setMaxQueueSize(8);
        server.setStatusPersistencePath(metricsPath);

        server.init();
        const int listenPort = server.getListenPort();
        ASSERT_GT(listenPort, 0);

        std::atomic<bool> finished{false};
        std::thread t([&] {
            server.start();
            finished.store(true, std::memory_order_release);
        });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            if (server.isRunning()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ASSERT_TRUE(server.isRunning()) << "accept loop should have set running";

        server.stop();
        // Unblock accept() without waiting for SO_RCVTIMEO (TIMEOUT_SEC).
        wakeAcceptOnLocalhost(listenPort);
        t.join();

        EXPECT_TRUE(finished.load(std::memory_order_acquire));
    }

    std::error_code ec;
    fs::remove_all(tmpDir, ec);
}
