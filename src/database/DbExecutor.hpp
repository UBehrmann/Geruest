#ifndef GERUEST_DB_EXECUTOR_HPP
#define GERUEST_DB_EXECUTOR_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <type_traits>
#include <utility>

namespace geruest::db {

class DbExecutor {
   public:
    explicit DbExecutor(std::size_t threadCount);
    ~DbExecutor();
    void join();

    template <typename Fn>
    auto run(Fn&& fn);

   private:
    boost::asio::thread_pool _pool;
    bool _joined = false;
};

template <typename Fn>
auto DbExecutor::run(Fn&& fn) {
    using ReturnType = typename std::invoke_result_t<Fn>;
    if constexpr (std::is_void_v<ReturnType>) {
        return boost::asio::co_spawn(
            _pool.get_executor(),
            [task = std::forward<Fn>(fn)]() mutable -> boost::asio::awaitable<void> {
                task();
                co_return;
            },
            boost::asio::use_awaitable);
    } else {
        return boost::asio::co_spawn(
            _pool.get_executor(),
            [task = std::forward<Fn>(fn)]() mutable -> boost::asio::awaitable<ReturnType> {
                co_return task();
            },
            boost::asio::use_awaitable);
    }
}

}  // namespace geruest::db

#endif  // GERUEST_DB_EXECUTOR_HPP
