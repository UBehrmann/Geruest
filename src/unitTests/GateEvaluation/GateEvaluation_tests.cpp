#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <optional>
#include <thread>

#include "data/HTTPRequest.hpp"
#include "handler/GateEvaluation.hpp"
#include "handler/SyncGateExecutor.hpp"

using namespace geruest;

namespace {

HTTPRequest makeGetRequest(const std::string& path) {
    const std::string raw = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    return HTTPRequest(std::move(raw), "127.0.0.1", "/tmp");
}

}  // namespace

TEST(GateEvaluation, SyncGateRunsOffIoThread) {
    boost::asio::io_context io;
    const auto ioThreadId = std::this_thread::get_id();
    std::optional<std::thread::id> gateThreadId;

    ResolvedRouteGate gate;
    gate.async = false;
    gate.syncHandler = [&](const HTTPRequest&) {
        gateThreadId = std::this_thread::get_id();
        return true;
    };

    auto future = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            const bool allowed = co_await evaluateResolvedGateAsync(
                gate, makeGetRequest("/v1/test"), [](const std::string&) {}, "route");
            EXPECT_TRUE(allowed);
        }(),
        boost::asio::use_future);
    io.run();
    future.get();

    ASSERT_TRUE(gateThreadId.has_value());
    EXPECT_NE(*gateThreadId, ioThreadId);
}
