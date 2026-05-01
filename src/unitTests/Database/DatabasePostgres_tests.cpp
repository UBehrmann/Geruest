#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "database/DatabaseClient.hpp"

namespace {

std::string envOr(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && v[0]) ? v : fallback;
}

}  // namespace

TEST(DatabasePostgres, QueryAndConcurrentInsert) {
#if GERUEST_HAS_LIBPQ
    geruest::db::PostgresConfig cfg;
    cfg.host     = envOr("POSTGRES_HOST", "localhost");
    cfg.port     = std::atoi(envOr("POSTGRES_PORT", "5432").c_str());
    cfg.database = envOr("POSTGRES_DB", "geruest_test");
    cfg.user     = envOr("POSTGRES_USER", "geruest");
    cfg.password = envOr("POSTGRES_PASSWORD", "geruest");
    cfg.sslmode  = "disable";
    cfg.connectTimeoutSeconds = 5;

    geruest::db::CommonConfig common;
    common.poolSize = 2;

    auto client = geruest::db::createPostgresClient(cfg, common);
    ASSERT_NE(client, nullptr);

    boost::asio::io_context io;

    auto setupFuture = boost::asio::co_spawn(
        io,
        [client]() -> boost::asio::awaitable<void> {
            co_await client->executeAsync("DROP TABLE IF EXISTS pg_test_users", {});
            co_await client->executeAsync(
                "CREATE TABLE pg_test_users (id SERIAL PRIMARY KEY, name TEXT NOT NULL)", {});
            co_return;
        },
        boost::asio::use_future);
    io.run();
    setupFuture.get();

    io.restart();
    auto f1 = boost::asio::co_spawn(
        io,
        [client]() -> boost::asio::awaitable<void> {
            co_await client->executeAsync(
                "INSERT INTO pg_test_users(name) VALUES($1)", {std::string("alice")});
            co_return;
        },
        boost::asio::use_future);
    auto f2 = boost::asio::co_spawn(
        io,
        [client]() -> boost::asio::awaitable<void> {
            co_await client->executeAsync(
                "INSERT INTO pg_test_users(name) VALUES($1)", {std::string("bob")});
            co_return;
        },
        boost::asio::use_future);
    io.run();
    f1.get();
    f2.get();

    io.restart();
    auto queryFuture = boost::asio::co_spawn(
        io,
        [client]() -> boost::asio::awaitable<geruest::db::QueryResult> {
            co_return co_await client->queryAsync(
                "SELECT name FROM pg_test_users ORDER BY id ASC", {});
        },
        boost::asio::use_future);
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

    io.restart();
    auto cleanupFuture = boost::asio::co_spawn(
        io,
        [client]() -> boost::asio::awaitable<void> {
            co_await client->executeAsync("DROP TABLE IF EXISTS pg_test_users", {});
            co_return;
        },
        boost::asio::use_future);
    io.run();
    cleanupFuture.get();
#else
    GTEST_SKIP() << "PostgreSQL backend not enabled";
#endif
}
