/**
 * @file SyncGateExecutor.hpp
 * @brief Thread pool for sync page/route gate handlers (keeps io_context workers free).
 */

#ifndef GERUEST_SYNCGATEEXECUTOR_HPP
#define GERUEST_SYNCGATEEXECUTOR_HPP

#include <boost/asio/thread_pool.hpp>

namespace geruest {

/** Process-wide pool; ponytail: fixed size — make configurable on Geruest if needed. */
boost::asio::thread_pool& syncGateThreadPool();

}  // namespace geruest

#endif  // GERUEST_SYNCGATEEXECUTOR_HPP
