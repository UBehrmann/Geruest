/**
 * @file SyncGateExecutor.cpp
 */

#include "SyncGateExecutor.hpp"

#include <algorithm>
#include <thread>

namespace geruest {

boost::asio::thread_pool& syncGateThreadPool() {
    static boost::asio::thread_pool pool(
        std::max<std::size_t>(2U, std::thread::hardware_concurrency()));
    return pool;
}

}  // namespace geruest
