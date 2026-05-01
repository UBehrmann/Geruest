#ifndef GERUEST_DB_EXECUTOR_HPP
#define GERUEST_DB_EXECUTOR_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

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
    using ResultStorage = std::conditional_t<std::is_void_v<ReturnType>, std::monostate, ReturnType>;
    auto ex = co_await boost::asio::this_coro::executor;
    auto error = std::make_shared<std::exception_ptr>();
    auto result = std::make_shared<std::optional<ResultStorage>>();

    auto done = std::make_shared<boost::asio::steady_timer>(ex);
    done->expires_at((boost::asio::steady_timer::time_point::max)());

    std::cerr << "[geruest::DbExecutor] posting task to pool ..." << std::endl;
    boost::asio::post(_pool, [ex, task = std::forward<Fn>(fn), error, result, done]() mutable {
        std::cerr << "[geruest::DbExecutor] pool thread running task ..." << std::endl;
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                task();
            } else {
                result->emplace(task());
            }
        } catch (...) {
            *error = std::current_exception();
            std::cerr << "[geruest::DbExecutor] pool thread task threw exception" << std::endl;
        }

        std::cerr << "[geruest::DbExecutor] pool thread posting cancel to strand ..." << std::endl;
        boost::asio::post(ex, [done]() mutable {
            std::cerr << "[geruest::DbExecutor] strand running cancel" << std::endl;
            done->cancel();
        });
        std::cerr << "[geruest::DbExecutor] pool thread done" << std::endl;
    });

    std::cerr << "[geruest::DbExecutor] co_await timer ..." << std::endl;
    boost::system::error_code waitEc;
    co_await done->async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, waitEc));
    std::cerr << "[geruest::DbExecutor] timer woke up, ec=" << waitEc.message() << std::endl;

    if (*error) {
        std::cerr << "[geruest::DbExecutor] rethrowing exception" << std::endl;
        std::rethrow_exception(*error);
    }

    if constexpr (std::is_void_v<ReturnType>) {
        co_return;
    } else {
        co_return std::move(**result);
    }
}

}  // namespace geruest::db

#endif  // GERUEST_DB_EXECUTOR_HPP
