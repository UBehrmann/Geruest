#ifndef GERUEST_DATABASE_CLIENT_HPP
#define GERUEST_DATABASE_CLIENT_HPP

#include "geruest/BuildConfig.hpp"

#include <boost/asio/awaitable.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DatabaseTypes.hpp"
#include "parser/JSONParser.hpp"

namespace geruest::db {

/** Build JSON object `{"rows":[...], "affectedRows":N}` from a query result. */
geruest::JSONParser toJSONParser(const QueryResult& result);

class DatabaseClient {
   public:
    virtual ~DatabaseClient() = default;

    virtual Backend backend() const = 0;
    virtual boost::asio::awaitable<QueryResult> queryAsync(std::string sql,
                                                           std::vector<BindValue> params) = 0;
    virtual boost::asio::awaitable<std::uint64_t> executeAsync(std::string sql,
                                                               std::vector<BindValue> params) = 0;

    /** Same SQL/params as queryAsync; returns toJSONParser(result). */
    boost::asio::awaitable<geruest::JSONParser> queryJsonAsync(std::string sql,
                                                                 std::vector<BindValue> params);
};

#if GERUEST_HAS_LIBPQ
struct PostgresConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database;
    std::string user;
    std::string password;
    std::string sslmode = "prefer";
    int connectTimeoutSeconds = 5;
    int statementTimeoutMs = 30000;
    bool tcpKeepalives = true;
    int keepalivesIdleSeconds = 60;
    int keepalivesIntervalSeconds = 10;
    int keepalivesCount = 3;
    /** Max statements per libpq pipeline batch per connection worker (1 = pipeline off). */
    unsigned maxPipelineBatch = 8;
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
