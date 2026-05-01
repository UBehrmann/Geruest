#include "DatabaseClient.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

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

#if GERUEST_HAS_LIBPQ
inline int pgPollTimeoutMs(int statementTimeoutMs) {
    return statementTimeoutMs > 0 ? statementTimeoutMs : 30000;
}

void pgWaitSocket(PGconn* conn, short events, int pollTimeoutMs) {
    const int fd = PQsocket(conn);
    if (fd < 0) {
        throw std::runtime_error("postgres socket unavailable");
    }
    struct pollfd pfd {
        fd, events, 0
    };
    while (true) {
        const int rc = ::poll(&pfd, 1, pollTimeoutMs);
        if (rc > 0) {
            return;
        }
        if (rc == 0) {
            throw std::runtime_error("postgres socket wait timeout");
        }
        if (errno == EINTR) {
            continue;
        }
        throw std::runtime_error("postgres socket wait failed");
    }
}

void pgFlushConn(PGconn* conn, int pollTimeoutMs) {
    while (PQflush(conn) == 1) {
        pgWaitSocket(conn, POLLOUT, pollTimeoutMs);
    }
}

void pgBindParams(const std::vector<BindValue>& params,
                  std::vector<std::string>& paramStorage,
                  std::vector<const char*>& values,
                  std::vector<int>& lengths,
                  std::vector<int>& formats,
                  std::vector<Oid>& types) {
    paramStorage.clear();
    values.clear();
    lengths.assign(params.size(), 0);
    formats.assign(params.size(), 0);
    types.assign(params.size(), 0);
    paramStorage.reserve(params.size());
    values.reserve(params.size());
    for (const auto& p : params) {
        if (std::holds_alternative<std::nullptr_t>(p)) {
            paramStorage.emplace_back();
            values.push_back(nullptr);
        } else {
            paramStorage.push_back(bindValueToString(p));
            values.push_back(paramStorage.back().c_str());
        }
    }
}

PGresult* pgReceiveFinalResult(PGconn* conn, int pollTimeoutMs) {
    PGresult* res = nullptr;
    while (true) {
        while (PQisBusy(conn) == 1) {
            pgWaitSocket(conn, POLLIN, pollTimeoutMs);
            if (PQconsumeInput(conn) == 0) {
                const std::string err = PQerrorMessage(conn);
                if (res != nullptr) {
                    PQclear(res);
                }
                throw std::runtime_error("postgres consume input failed: " + err);
            }
        }
        PGresult* next = PQgetResult(conn);
        if (next == nullptr) {
            break;
        }
        if (res != nullptr) {
            PQclear(res);
        }
        res = next;
    }
    if (res == nullptr) {
        const std::string err = PQerrorMessage(conn);
        throw std::runtime_error("postgres query returned no result: " + err);
    }
    return res;
}

QueryResult pgBuildQueryResult(PGresult* res) {
    const ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        const std::string err = PQresultErrorMessage(res);
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
    return out;
}

PGconn* pgConnectOne(const std::string& conninfo, int statementTimeoutMs) {
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        const std::string err = PQerrorMessage(conn);
        PQfinish(conn);
        throw std::runtime_error("Failed to connect postgres: " + err);
    }
    if (PQsetnonblocking(conn, 1) != 0) {
        const std::string err = PQerrorMessage(conn);
        PQfinish(conn);
        throw std::runtime_error("Failed to set postgres nonblocking mode: " + err);
    }
    if (statementTimeoutMs > 0) {
        const std::string setStmt = "SET statement_timeout = " + std::to_string(statementTimeoutMs);
        PGresult* r = PQexec(conn, setStmt.c_str());
        PQclear(r);
    }
    return conn;
}

void pgEnsurePipelineOff(PGconn* conn) {
    if (PQpipelineStatus(conn) == PQ_PIPELINE_ON) {
        PQexitPipelineMode(conn);
    }
}

#endif  // GERUEST_HAS_LIBPQ

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
        auto self = shared_from_this();
        auto sqlPtr = std::make_shared<std::string>(std::move(sql));
        auto paramsPtr = std::make_shared<std::vector<BindValue>>(std::move(params));
        return _executor.run([self = std::move(self), sqlPtr = std::move(sqlPtr), paramsPtr = std::move(paramsPtr)]() {
            sqlite3* conn = self->_pool.acquire();
            sqlite3_stmt* stmt = nullptr;
            QueryResult result;

            auto releaseConn = [self, conn]() { self->_pool.release(conn); };
            auto finalizeStmt = [&stmt]() {
                if (stmt != nullptr) {
                    sqlite3_finalize(stmt);
                }
            };

            if (sqlite3_prepare_v2(conn, sqlPtr->c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                const std::string err = sqlite3_errmsg(conn);
                releaseConn();
                throw std::runtime_error("sqlite prepare failed: " + err);
            }

            for (std::size_t i = 0; i < paramsPtr->size(); ++i) {
                const int index = static_cast<int>(i + 1);
                const auto& value = (*paramsPtr)[i];
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
        : _statementTimeoutMs(config.statementTimeoutMs)
        , _maxPipelineBatch(std::max(1u, config.maxPipelineBatch)) {
        const std::size_t poolSize = std::max<std::size_t>(common.poolSize, 1);
        std::ostringstream conninfo;
        conninfo << "host=" << config.host
                 << " port=" << config.port
                 << " dbname=" << config.database
                 << " user=" << config.user
                 << " password=" << config.password
                 << " sslmode=" << config.sslmode
                 << " connect_timeout=" << config.connectTimeoutSeconds;
        if (config.tcpKeepalives) {
            conninfo << " keepalives=1"
                     << " keepalives_idle=" << config.keepalivesIdleSeconds
                     << " keepalives_interval=" << config.keepalivesIntervalSeconds
                     << " keepalives_count=" << config.keepalivesCount;
        }
        _conninfo = conninfo.str();

        _conns.reserve(poolSize);
        for (std::size_t i = 0; i < poolSize; ++i) {
            _conns.push_back(pgConnectOne(_conninfo, _statementTimeoutMs));
        }
        _workers.reserve(poolSize);
        for (std::size_t i = 0; i < poolSize; ++i) {
            _workers.emplace_back([this, i] { workerMain(static_cast<std::size_t>(i)); });
        }
    }

    ~PostgresClient() override {
        {
            std::lock_guard<std::mutex> lock(_qMu);
            _stop.store(true, std::memory_order_release);
            while (!_queue.empty()) {
                std::unique_ptr<WorkItem> w = std::move(_queue.front());
                _queue.pop_front();
                completeItem(w, std::make_exception_ptr(std::runtime_error("postgres client shutting down")), std::nullopt);
            }
        }
        _qCv.notify_all();
        for (std::thread& t : _workers) {
            if (t.joinable()) {
                t.join();
            }
        }
        for (PGconn* c : _conns) {
            if (c != nullptr) {
                PQfinish(c);
            }
        }
        _conns.clear();
    }

    Backend backend() const override { return Backend::Postgres; }

    boost::asio::awaitable<QueryResult> queryAsync(std::string sql,
                                                   std::vector<BindValue> params) override {
        auto ex = co_await boost::asio::this_coro::executor;
        auto done = std::make_shared<boost::asio::steady_timer>(ex);
        done->expires_at((boost::asio::steady_timer::time_point::max)());
        auto result = std::make_shared<std::optional<QueryResult>>();
        auto error = std::make_shared<std::exception_ptr>();

        auto sqlPtr = std::make_shared<std::string>(std::move(sql));
        auto paramsPtr = std::make_shared<std::vector<BindValue>>(std::move(params));
        auto item = std::make_unique<WorkItem>();
        item->sql = std::move(sqlPtr);
        item->params = std::move(paramsPtr);
        item->complete = [ex, done, result, error](std::exception_ptr err, std::optional<QueryResult> qr) mutable {
            boost::asio::post(ex, [done, result, error, err = std::move(err), qr = std::move(qr)]() mutable {
                *error = std::move(err);
                if (qr.has_value()) {
                    *result = std::move(*qr);
                }
                done->cancel();
            });
        };
        {
            std::lock_guard<std::mutex> lock(_qMu);
            if (_stop.load(std::memory_order_acquire)) {
                throw std::runtime_error("postgres client shutting down");
            }
            _queue.push_back(std::move(item));
        }
        _qCv.notify_one();

        boost::system::error_code waitEc;
        co_await done->async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, waitEc));
        if (*error) {
            std::rethrow_exception(*error);
        }
        if (!result->has_value()) {
            throw std::runtime_error("postgres query completed without result");
        }
        co_return std::move(**result);
    }

    boost::asio::awaitable<std::uint64_t> executeAsync(std::string sql,
                                                       std::vector<BindValue> params) override {
        QueryResult result = co_await queryAsync(std::move(sql), std::move(params));
        co_return result.affectedRows;
    }

   private:
    struct WorkItem {
        std::shared_ptr<std::string> sql;
        std::shared_ptr<std::vector<BindValue>> params;
        std::function<void(std::exception_ptr, std::optional<QueryResult>)> complete;
    };

    void workerMain(std::size_t slotIndex) {
        PGconn*& conn = _conns[slotIndex];
        const int pollMs = pgPollTimeoutMs(_statementTimeoutMs);
        while (true) {
            std::vector<std::unique_ptr<WorkItem>> batch;
            {
                std::unique_lock<std::mutex> lock(_qMu);
                _qCv.wait(lock, [this] { return _stop.load(std::memory_order_acquire) || !_queue.empty(); });
                if (_stop.load(std::memory_order_acquire) && _queue.empty()) {
                    return;
                }
                const std::size_t take = std::min<std::size_t>(static_cast<std::size_t>(_maxPipelineBatch), _queue.size());
                batch.reserve(take);
                for (std::size_t i = 0; i < take; ++i) {
                    batch.push_back(std::move(_queue.front()));
                    _queue.pop_front();
                }
            }

            try {
                runBatchOnConn(conn, batch, pollMs);
            } catch (...) {
                for (auto& w : batch) {
                    completeItem(w, std::current_exception(), std::nullopt);
                }
            }
        }
    }

    void runBatchOnConn(PGconn*& conn, std::vector<std::unique_ptr<WorkItem>>& batch, int pollMs) {
        if (batch.empty()) {
            return;
        }
        if (batch.size() == 1 || _maxPipelineBatch <= 1) {
            for (auto& w : batch) {
                runOneItem(conn, *w, pollMs);
            }
            return;
        }

        pgEnsurePipelineOff(conn);
        if (PQenterPipelineMode(conn) != 1) {
            for (auto& w : batch) {
                runOneItem(conn, *w, pollMs);
            }
            return;
        }

        for (auto& w : batch) {
            std::vector<std::string> paramStorage;
            std::vector<const char*> values;
            std::vector<int> lengths;
            std::vector<int> formats;
            std::vector<Oid> types;
            pgBindParams(*w->params, paramStorage, values, lengths, formats, types);
            if (PQsendQueryParams(conn, w->sql->c_str(), static_cast<int>(values.size()), types.data(), values.data(),
                                  lengths.data(), formats.data(), 0) == 0) {
                const std::string err = PQerrorMessage(conn);
                PQreset(conn);
                reconnectConn(conn);
                throw std::runtime_error("postgres pipeline send failed: " + err);
            }
        }

        if (PQpipelineSync(conn) == 0) {
            const std::string err = PQerrorMessage(conn);
            PQreset(conn);
            reconnectConn(conn);
            throw std::runtime_error("postgres pipeline sync failed: " + err);
        }

        pgFlushConn(conn, pollMs);

        std::vector<QueryResult> results;
        results.reserve(batch.size());
        while (results.size() < batch.size()) {
            PGresult* res = pgReceiveFinalResult(conn, pollMs);
            const ExecStatusType st = PQresultStatus(res);
            if (st == PGRES_PIPELINE_SYNC) {
                PQclear(res);
                continue;
            }
            try {
                results.push_back(pgBuildQueryResult(res));
            } catch (...) {
                PQclear(res);
                throw;
            }
            PQclear(res);
        }
        for (std::size_t i = 0; i < batch.size(); ++i) {
            completeItem(batch[i], nullptr, std::move(results[i]));
        }

        pgEnsurePipelineOff(conn);
    }

    void runOneItem(PGconn*& conn, WorkItem& w, int pollMs) {
        pgEnsurePipelineOff(conn);
        std::vector<std::string> paramStorage;
        std::vector<const char*> values;
        std::vector<int> lengths;
        std::vector<int> formats;
        std::vector<Oid> types;
        pgBindParams(*w.params, paramStorage, values, lengths, formats, types);
        if (PQsendQueryParams(conn, w.sql->c_str(), static_cast<int>(values.size()), types.data(), values.data(), lengths.data(),
                              formats.data(), 0) == 0) {
            const std::string err = PQerrorMessage(conn);
            PQreset(conn);
            reconnectConn(conn);
            throw std::runtime_error("postgres query failed: " + err);
        }
        pgFlushConn(conn, pollMs);
        PGresult* res = pgReceiveFinalResult(conn, pollMs);
        try {
            completeItem(w, nullptr, pgBuildQueryResult(res));
        } catch (...) {
            completeItem(w, std::current_exception(), std::nullopt);
        }
        PQclear(res);
    }

    static void completeItem(std::unique_ptr<WorkItem>& item, std::exception_ptr err, std::optional<QueryResult> result) {
        if (!item || !item->complete) {
            return;
        }
        item->complete(std::move(err), std::move(result));
    }

    static void completeItem(WorkItem& item, std::exception_ptr err, std::optional<QueryResult> result) {
        if (!item.complete) {
            return;
        }
        item.complete(std::move(err), std::move(result));
    }

    void reconnectConn(PGconn*& conn) {
        if (conn != nullptr) {
            PQfinish(conn);
            conn = nullptr;
        }
        conn = pgConnectOne(_conninfo, _statementTimeoutMs);
    }

    std::string _conninfo;
    int _statementTimeoutMs;
    unsigned _maxPipelineBatch;
    std::vector<PGconn*> _conns;

    std::mutex _qMu;
    std::condition_variable _qCv;
    std::deque<std::unique_ptr<WorkItem>> _queue;
    std::atomic<bool> _stop{false};
    std::vector<std::thread> _workers;
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
