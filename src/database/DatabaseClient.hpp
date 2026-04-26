#ifndef GERUEST_DATABASE_CLIENT_HPP
#define GERUEST_DATABASE_CLIENT_HPP

#include <boost/asio/awaitable.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DatabaseTypes.hpp"

namespace geruest::db {

class DatabaseClient {
   public:
    virtual ~DatabaseClient() = default;

    virtual Backend backend() const = 0;
    virtual boost::asio::awaitable<QueryResult> queryAsync(std::string sql,
                                                           std::vector<BindValue> params) = 0;
    virtual boost::asio::awaitable<std::uint64_t> executeAsync(std::string sql,
                                                               std::vector<BindValue> params) = 0;
};

#if GERUEST_HAS_LIBPQ
struct PostgresConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database;
    std::string user;
    std::string password;
    std::string sslmode = "prefer";
};
#endif

#if GERUEST_HAS_SQLITE
struct SqliteConfig {
    std::string path;
    int busyTimeoutMs = 5000;
};
#endif

struct CommonConfig {
    std::size_t poolSize = 4;
    std::size_t sqliteExecutorThreads = 1;
};

#if GERUEST_HAS_LIBPQ
std::shared_ptr<DatabaseClient> createPostgresClient(const PostgresConfig& config, const CommonConfig& common);
#endif

#if GERUEST_HAS_SQLITE
std::shared_ptr<DatabaseClient> createSqliteClient(const SqliteConfig& config, const CommonConfig& common);
#endif

}  // namespace geruest::db

#endif  // GERUEST_DATABASE_CLIENT_HPP
