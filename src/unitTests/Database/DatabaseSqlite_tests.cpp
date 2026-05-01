#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdio>
#include <filesystem>
#include <future>
#include <string>
#include <vector>
#include <algorithm>

#include "database/DatabaseClient.hpp"

namespace {

std::string tempSqlitePath() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() / ("geruest_db_test_" + std::to_string(now) + ".db")).string();
}

boost::asio::awaitable<void> createUsersTable(const std::shared_ptr<geruest::db::DatabaseClient>& client) {
    co_await client->executeAsync(
        "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)", {});
    co_return;
}

boost::asio::awaitable<void> insertAlice(const std::shared_ptr<geruest::db::DatabaseClient>& client) {
    co_await client->executeAsync("INSERT INTO users(name) VALUES('alice')", {});
    co_return;
}

boost::asio::awaitable<void> insertBob(const std::shared_ptr<geruest::db::DatabaseClient>& client) {
    co_await client->executeAsync("INSERT INTO users(name) VALUES('bob')", {});
    co_return;
}

boost::asio::awaitable<geruest::db::QueryResult> queryUsersById(
    const std::shared_ptr<geruest::db::DatabaseClient>& client) {
    co_return co_await client->queryAsync("SELECT name FROM users ORDER BY id ASC", {});
}

}  // namespace

TEST(DatabaseSqlite, QueryAndConcurrentInsert) {
#if GERUEST_HAS_SQLITE
    const std::string dbPath = tempSqlitePath();

    geruest::db::SqliteConfig cfg;
    cfg.path = dbPath;
    cfg.busyTimeoutMs = 4000;

    geruest::db::CommonConfig common;
    common.poolSize = 2;
    common.sqliteExecutorThreads = 2;

    auto client = geruest::db::createSqliteClient(cfg, common);
    ASSERT_NE(client, nullptr);

    boost::asio::io_context io;
    auto setupFuture = boost::asio::co_spawn(io, createUsersTable(client), boost::asio::use_future);
    io.run();
    setupFuture.get();

    io.restart();
    auto f1 = boost::asio::co_spawn(io, insertAlice(client), boost::asio::use_future);
    auto f2 = boost::asio::co_spawn(io, insertBob(client), boost::asio::use_future);
    io.run();
    f1.get();
    f2.get();

    io.restart();
    auto queryFuture = boost::asio::co_spawn(io, queryUsersById(client), boost::asio::use_future);
    io.run();
    geruest::db::QueryResult result = queryFuture.get();
    ASSERT_EQ(result.rows.size(), 2u);
    std::vector<std::string> names;
    names.reserve(result.rows.size());
    for (const auto& row : result.rows) {
        ASSERT_FALSE(row.columns.empty());
        names.push_back(row.columns[0]);
    }
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names[0], "alice");
    EXPECT_EQ(names[1], "bob");

    std::remove(dbPath.c_str());
#else
    GTEST_SKIP() << "SQLite backend not enabled";
#endif
}
