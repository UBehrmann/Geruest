/**
 * @file GateEvaluation.hpp
 * @brief Shared sync/async resolved gate handler invocation.
 */

#ifndef GERUEST_GATEEVALUATION_HPP
#define GERUEST_GATEEVALUATION_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <functional>
#include <string>
#include <string_view>

#include "SyncGateExecutor.hpp"
#include "data/HTTPRequest.hpp"
#include "data/ServerTypes.hpp"

namespace geruest {

/** Invoke a resolved page or route gate; returns true when access is granted. */
template <typename ResolvedGate>
boost::asio::awaitable<bool> evaluateResolvedGateAsync(
    const ResolvedGate& gate, const HTTPRequest& request,
    const std::function<void(const std::string&)>& logError, std::string_view gateKind) {
    bool allowed = false;
    try {
        if (gate.async) {
            allowed = co_await gate.asyncHandler(request);
        } else {
            auto handler = gate.syncHandler;
            allowed = co_await boost::asio::co_spawn(
                syncGateThreadPool().get_executor(),
                [handler, &request]() -> boost::asio::awaitable<bool> { co_return handler(request); },
                boost::asio::use_awaitable);
        }
    } catch (const std::exception& e) {
        logError(std::string("Exception in ") + std::string(gateKind) + " gate handler: " + e.what());
    } catch (...) {
        logError(std::string("Unknown exception in ") + std::string(gateKind) + " gate handler");
    }
    co_return allowed;
}

}  // namespace geruest

#endif  // GERUEST_GATEEVALUATION_HPP
