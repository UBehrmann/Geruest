#include "DatabaseClient.hpp"

#include <boost/asio/co_spawn.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <stdexcept>

#include "DbExecutor.hpp"

#if GERUEST_HAS_LIBPQ
#include <libpq-fe.h>
#include <postgres_ext.h>
#endif

#if GERUEST_HAS_SQLITE
#include <sqlite3.h>
#endif

namespace geruest::db {

namespace {

inline std::string bindValueToString(const BindValue& v) {
    if (std::holds_alternative<std::nullptr_t>(v)) {
        return "";
    }
    if (std::holds_alternative<std::int64_t>(v)) {
        return std::to_string(std::get<std::int64_t>(v));
    }
    if (std::holds_alternative<double>(v)) {
        std::ostringstream oss;
        oss << std::get<double>(v);
        return oss.str();
    }
    return std::get<std::string>(v);
}

template <typename T>
class SimplePool {
   public:
    void add(T item) { _items.push_back(std::move(item)); }

    T acquire() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return !_items.empty(); });
        T out = std::move(_items.back());
        _items.pop_back();
        return out;
    }

    void release(T item) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _items.push_back(std::move(item));
        }
        _cv.notify_one();
    }

   private:
    std::vector<T> _items;
    std::mutex _mutex;
    std::condition_variable _cv;
};

}  // namespace

#if GERUEST_HAS_SQLITE
class SqliteClient final : public std::enable_shared_from_this<SqliteClient>, public DatabaseClient {
   public:
    SqliteClient(const SqliteConfig& config, const CommonConfig& common)
        : _executor(std::max<std::size_t>(common.sqliteExecutorThreads, 1)) {
        const std::size_t poolSize = std::max<std::size_t>(common.poolSize, 1);
        for (std::size_t i = 0; i < poolSize; ++i) {
            sqlite3* conn = nullptr;
            if (sqlite3_open(config.path.c_str(), &conn) != SQLITE_OK) {
                const std::string err = conn != nullptr ? sqlite3_errmsg(conn) : "unknown sqlite open error";
                if (conn != nullptr) {
                    sqlite3_close(conn);
                }
                throw std::runtime_error("Failed to open sqlite database: " + err);
            }
            sqlite3_exec(conn, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            sqlite3_busy_timeout(conn, config.busyTimeoutMs);
            _pool.add(conn);
            _allConnections.push_back(conn);
        }
    }

    ~SqliteClient() override {
        _executor.join();
        for (sqlite3* conn : _allConnections) {
            sqlite3_close(conn);
        }
    }

    Backend backend() const override { return Backend::Sqlite; }

    boost::asio::awaitable<QueryResult> queryAsync(std::string sql,
                                                   std::vector<BindValue> params) override {
        std::shared_ptr<SqliteClient> self = shared_from_this();
        co_return co_await _executor.run([self = std::move(self), sql = std::move(sql), params = std::move(params)]() {
            sqlite3* conn = self->_pool.acquire();
            sqlite3_stmt* stmt = nullptr;
            QueryResult result;

            auto releaseConn = [self, conn]() { self->_pool.release(conn); };
            auto finalizeStmt = [&stmt]() {
                if (stmt != nullptr) {
                    sqlite3_finalize(stmt);
                }
            };

            if (sqlite3_prepare_v2(conn, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                const std::string err = sqlite3_errmsg(conn);
                releaseConn();
                throw std::runtime_error("sqlite prepare failed: " + err);
            }

            for (std::size_t i = 0; i < params.size(); ++i) {
                const int index = static_cast<int>(i + 1);
                const auto& value = params[i];
                int rc = SQLITE_OK;
                if (std::holds_alternative<std::nullptr_t>(value)) {
                    rc = sqlite3_bind_null(stmt, index);
                } else if (std::holds_alternative<std::int64_t>(value)) {
                    rc = sqlite3_bind_int64(stmt, index, std::get<std::int64_t>(value));
                } else if (std::holds_alternative<double>(value)) {
                    rc = sqlite3_bind_double(stmt, index, std::get<double>(value));
                } else {
                    const std::string& text = std::get<std::string>(value);
                    rc = sqlite3_bind_text(stmt, index, text.c_str(), -1, SQLITE_TRANSIENT);
                }
                if (rc != SQLITE_OK) {
                    const std::string err = sqlite3_errmsg(conn);
                    finalizeStmt();
                    releaseConn();
                    throw std::runtime_error("sqlite bind failed: " + err);
                }
            }

            const int colCount = sqlite3_column_count(stmt);
            result.columnNames.reserve(static_cast<std::size_t>(colCount));
            for (int col = 0; col < colCount; ++col) {
                result.columnNames.emplace_back(sqlite3_column_name(stmt, col));
            }

            while (true) {
                const int step = sqlite3_step(stmt);
                if (step == SQLITE_ROW) {
                    QueryRow row;
                    row.columns.reserve(static_cast<std::size_t>(colCount));
                    for (int col = 0; col < colCount; ++col) {
                        const unsigned char* txt = sqlite3_column_text(stmt, col);
                        row.columns.emplace_back(txt == nullptr ? "" : reinterpret_cast<const char*>(txt));
                    }
                    result.rows.push_back(std::move(row));
                    continue;
                }
                if (step == SQLITE_DONE) {
                    break;
                }
                const std::string err = sqlite3_errmsg(conn);
                finalizeStmt();
                releaseConn();
                throw std::runtime_error("sqlite step failed: " + err);
            }

            result.affectedRows = static_cast<std::uint64_t>(sqlite3_changes(conn));
            finalizeStmt();
            releaseConn();
            return result;
        });
    }

    boost::asio::awaitable<std::uint64_t> executeAsync(std::string sql,
                                                       std::vector<BindValue> params) override {
        QueryResult result = co_await queryAsync(std::move(sql), std::move(params));
        co_return result.affectedRows;
    }

   private:
    DbExecutor _executor;
    SimplePool<sqlite3*> _pool;
    std::vector<sqlite3*> _allConnections;
};
#endif

#if GERUEST_HAS_LIBPQ
class PostgresClient final : public std::enable_shared_from_this<PostgresClient>, public DatabaseClient {
   public:
    PostgresClient(const PostgresConfig& config, const CommonConfig& common)
        : _executor(std::max<std::size_t>(common.poolSize, 1)) {
        const std::size_t poolSize = std::max<std::size_t>(common.poolSize, 1);
        for (std::size_t i = 0; i < poolSize; ++i) {
            std::ostringstream conninfo;
            conninfo << "host=" << config.host
                     << " port=" << config.port
                     << " dbname=" << config.database
                     << " user=" << config.user
                     << " password=" << config.password
                     << " sslmode=" << config.sslmode;

            PGconn* conn = PQconnectdb(conninfo.str().c_str());
            if (PQstatus(conn) != CONNECTION_OK) {
                const std::string err = PQerrorMessage(conn);
                PQfinish(conn);
                throw std::runtime_error("Failed to connect postgres: " + err);
            }
            _pool.add(conn);
            _allConnections.push_back(conn);
        }
    }

    ~PostgresClient() override {
        _executor.join();
        for (PGconn* conn : _allConnections) {
            PQfinish(conn);
        }
    }

    Backend backend() const override { return Backend::Postgres; }

    boost::asio::awaitable<QueryResult> queryAsync(std::string sql,
                                                   std::vector<BindValue> params) override {
        std::shared_ptr<PostgresClient> self = shared_from_this();
        co_return co_await _executor.run([self = std::move(self), sql = std::move(sql), params = std::move(params)]() {
            PGconn* conn = self->_pool.acquire();
            auto releaseConn = [self, conn]() { self->_pool.release(conn); };

            std::vector<std::string> paramStorage;
            std::vector<const char*> values;
            std::vector<int> lengths;
            std::vector<int> formats;
            std::vector<Oid> types;
            paramStorage.reserve(params.size());
            values.reserve(params.size());
            lengths.assign(params.size(), 0);
            formats.assign(params.size(), 0);
            types.assign(params.size(), 0);

            for (const auto& p : params) {
                if (std::holds_alternative<std::nullptr_t>(p)) {
                    paramStorage.emplace_back();
                    values.push_back(nullptr);
                } else {
                    paramStorage.push_back(bindValueToString(p));
                    values.push_back(paramStorage.back().c_str());
                }
            }

            PGresult* res = PQexecParams(conn, sql.c_str(), static_cast<int>(values.size()), types.data(),
                                         values.data(), lengths.data(), formats.data(), 0);
            if (res == nullptr) {
                const std::string err = PQerrorMessage(conn);
                releaseConn();
                throw std::runtime_error("postgres query failed: " + err);
            }

            const ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
                const std::string err = PQresultErrorMessage(res);
                PQclear(res);
                releaseConn();
                throw std::runtime_error("postgres query failed: " + err);
            }

            QueryResult out;
            const int fields = PQnfields(res);
            const int rows = PQntuples(res);
            out.columnNames.reserve(static_cast<std::size_t>(fields));
            for (int c = 0; c < fields; ++c) {
                out.columnNames.emplace_back(PQfname(res, c));
            }
            out.rows.reserve(static_cast<std::size_t>(rows));
            for (int r = 0; r < rows; ++r) {
                QueryRow row;
                row.columns.reserve(static_cast<std::size_t>(fields));
                for (int c = 0; c < fields; ++c) {
                    if (PQgetisnull(res, r, c) == 1) {
                        row.columns.emplace_back();
                    } else {
                        row.columns.emplace_back(PQgetvalue(res, r, c));
                    }
                }
                out.rows.push_back(std::move(row));
            }

            const char* cmdTuples = PQcmdTuples(res);
            if (cmdTuples != nullptr && *cmdTuples != '\0') {
                out.affectedRows = static_cast<std::uint64_t>(std::strtoull(cmdTuples, nullptr, 10));
            }
            PQclear(res);
            releaseConn();
            return out;
        });
    }

    boost::asio::awaitable<std::uint64_t> executeAsync(std::string sql,
                                                       std::vector<BindValue> params) override {
        QueryResult result = co_await queryAsync(std::move(sql), std::move(params));
        co_return result.affectedRows;
    }

   private:
    DbExecutor _executor;
    SimplePool<PGconn*> _pool;
    std::vector<PGconn*> _allConnections;
};
#endif

#if GERUEST_HAS_LIBPQ
std::shared_ptr<DatabaseClient> createPostgresClient(const PostgresConfig& config, const CommonConfig& common) {
    return std::make_shared<PostgresClient>(config, common);
}
#endif

#if GERUEST_HAS_SQLITE
std::shared_ptr<DatabaseClient> createSqliteClient(const SqliteConfig& config, const CommonConfig& common) {
    return std::make_shared<SqliteClient>(config, common);
}
#endif

}  // namespace geruest::db
