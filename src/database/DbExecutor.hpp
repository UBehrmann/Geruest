#ifndef GERUEST_DB_EXECUTOR_HPP
#define GERUEST_DB_EXECUTOR_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace geruest::db {

class DbExecutor {
   public:
    explicit DbExecutor(std::size_t threadCount);
    ~DbExecutor();
    void join();

    template <typename Fn>
    auto run(Fn&& fn) -> boost::asio::awaitable<typename std::invoke_result_t<Fn>>;

   private:
    boost::asio::thread_pool _pool;
    bool _joined = false;
};

template <typename Fn>
auto DbExecutor::run(Fn&& fn) -> boost::asio::awaitable<typename std::invoke_result_t<Fn>> {
    using ReturnType = typename std::invoke_result_t<Fn>;

    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();

    boost::asio::post(_pool, [promise, task = std::forward<Fn>(fn)]() mutable {
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                task();
                promise->set_value();
            } else {
                promise->set_value(task());
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });

    auto ex = co_await boost::asio::this_coro::executor;
    boost::asio::steady_timer timer(ex);
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        timer.expires_after(std::chrono::milliseconds(1));
        co_await timer.async_wait(boost::asio::use_awaitable);
    }

    if constexpr (std::is_void_v<ReturnType>) {
        future.get();
        co_return;
    } else {
        co_return future.get();
    }
}

}  // namespace geruest::db

#endif  // GERUEST_DB_EXECUTOR_HPP
