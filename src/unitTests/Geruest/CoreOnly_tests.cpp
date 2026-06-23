/**
 * @file CoreOnly_tests.cpp
 * @brief Smoke test linking Geruest::Core only (no Assets/WebSocket modules required at link).
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "Geruest.hpp"
#include "geruest/BuildConfig.hpp"
#include "modules/ModuleHooks.hpp"

TEST(CoreOnlyTest, TextContentPassthroughReadsRawFile) {
    const std::string path = "/etc/hosts";
    const auto body = geruest::modules::readTextFileRaw(path);
    ASSERT_TRUE(body.has_value());
    EXPECT_FALSE(body->empty());
}

#if !GERUEST_ENABLE_ASSETS

TEST(CoreOnlyTest, ServerStartsAndStopsWithoutAssets) {
    geruest::Geruest server;
    server.setPort(0);
    server.setBindAddress("127.0.0.1");
    server.setWorkerThreadCount(1);
    server.addRoute("/ping", [](const geruest::HTTPRequest&) {
        geruest::HTTPResponse resp("200 OK");
        resp.setHeader("Content-Type", "text/plain");
        resp.setBody("pong");
        return resp;
    });
    ASSERT_TRUE(server.init());
    const int listenPort = server.getListenPort();
    ASSERT_GT(listenPort, 0);

    std::atomic<bool> finished{false};
    std::thread worker([&] {
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
    ASSERT_TRUE(server.isRunning());

    server.stop();
    worker.join();
    EXPECT_TRUE(finished.load(std::memory_order_acquire));
    EXPECT_FALSE(server.isRunning());
}

#endif  // !GERUEST_ENABLE_ASSETS
