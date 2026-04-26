# Database Usage

Async database access in Geruest with optional PostgreSQL and SQLite backends.

## Overview

- Routes are coroutine handlers (`addRoute`) and can `co_await` DB calls.
- Backends are optional at CMake build time.
- Runtime backend selection supports:
  - `none`
  - `postgres`
  - `sqlite`

## Build Options

Enable backends when configuring:

```bash
cmake -S . -B build -DGERUEST_ENABLE_POSTGRESQL=ON -DGERUEST_ENABLE_SQLITE=ON
cmake --build build
```

Compile-time flags exposed to users:

- `GERUEST_HAS_LIBPQ` (0/1)
- `GERUEST_HAS_SQLITE` (0/1)

## Runtime Configuration (.env)

```env
DATABASE_BACKEND=sqlite
DATABASE_POOL_MAX=4

SQLITE_PATH=./geruest.db
SQLITE_BUSY_TIMEOUT_MS=5000
SQLITE_DB_EXECUTOR_THREADS=1

# PostgreSQL (used only when DATABASE_BACKEND=postgres)
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_DB=app
POSTGRES_USER=app
POSTGRES_PASSWORD=secret
POSTGRES_SSLMODE=prefer
```

Selection precedence:

1. code setter (`setDatabaseBackend(...)`)
2. `DATABASE_BACKEND` env/.env
3. default `none`

If selected backend is not compiled in, startup fails fast.

## Runtime Configuration (Code API)

```cpp
geruest::Geruest server;

server.setDatabaseBackend(geruest::DatabaseBackend::Sqlite);
server.setDatabasePoolSize(4);
server.setSqliteExecutorThreadCount(1);

#if GERUEST_HAS_SQLITE
geruest::db::SqliteConfig sqliteCfg;
sqliteCfg.path = "./geruest.db";
sqliteCfg.busyTimeoutMs = 5000;
server.configureSqlite(sqliteCfg);
#endif

server.loadConfig(".env"); // optional: still loads unset values
```

For PostgreSQL:

```cpp
#if GERUEST_HAS_LIBPQ
server.setDatabaseBackend(geruest::DatabaseBackend::Postgres);

geruest::db::PostgresConfig pg;
pg.host = "localhost";
pg.port = 5432;
pg.database = "app";
pg.user = "app";
pg.password = "secret";
pg.sslmode = "prefer";
server.configurePostgres(pg);
#endif
```

## Route Example

`addRoute` handler is coroutine-based. Access DB via `request.database()`.

```cpp
server.addRoute("/users", [](const geruest::HTTPRequest& request) -> boost::asio::awaitable<geruest::HTTPResponse> {
    geruest::HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");

    auto db = request.database();
    if (!db) {
        response.setBody(R"({"error":"database not configured"})");
        co_return response;
    }

    auto result = co_await db->queryAsync(
        "SELECT id, name FROM users WHERE name = ?",
        {std::string("alice")}
    );

    std::string body = "[";
    for (size_t i = 0; i < result.rows.size(); ++i) {
        if (i > 0) body += ",";
        body += "{\"id\":\"" + result.rows[i].columns[0] + "\",\"name\":\"" + result.rows[i].columns[1] + "\"}";
    }
    body += "]";
    response.setBody(body);
    co_return response;
});
```

Write operation:

```cpp
std::uint64_t affected = co_await db->executeAsync(
    "INSERT INTO users(name) VALUES(?)",
    {std::string("alice")}
);
```

## Parameter Binding

- Always pass values in `params` vector.
- Do not concatenate user input into SQL strings.
- Supported bind value types:
  - `nullptr`
  - `std::int64_t`
  - `double`
  - `std::string`

## SQLite Notes

- SQLite itself is synchronous.
- Geruest runs SQLite work on dedicated DB executor threads.
- This keeps HTTP worker threads non-blocking.
- Concurrent write order is not guaranteed; design tests accordingly.

## PostgreSQL Notes

- Uses libpq backend when compiled with `GERUEST_ENABLE_POSTGRESQL=ON`.
- Configure host/port/db/user/password/sslmode via code or `.env`.

## Troubleshooting

- **No DB available in route**: `request.database()` is null. Check `DATABASE_BACKEND` and backend compile flags.
- **Startup throws backend not compiled**: selected backend requires build option ON and matching dev package.
- **SQLite test missing**: configure tests with `-DGERUEST_ENABLE_SQLITE=ON`.
