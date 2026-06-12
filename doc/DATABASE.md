# Database Usage

Async database access in Geruest with optional PostgreSQL and SQLite backends.

## Overview

- Use `addRoute` for sync handlers and `addRouteAsync` for coroutine handlers.
- DB calls are async APIs and should be used from `addRouteAsync` handlers.
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

If a backend option is `ON` but the dependency is not found (no `libpq-dev` / `libsqlite3-dev`), CMake continues with that backend disabled and `GERUEST_HAS_*` stays `0`.

Compile-time flags exposed to users:

- `GERUEST_HAS_LIBPQ` (0/1)
- `GERUEST_HAS_SQLITE` (0/1)

## Runtime Configuration (.env)

Keys below are read when `Geruest::loadConfig(...)` runs (or your app uses `ConfigLoader` the same way). PostgreSQL keys apply only when `DATABASE_BACKEND=postgres`; SQLite keys when `DATABASE_BACKEND=sqlite`.

```env
DATABASE_BACKEND=sqlite
DATABASE_POOL_MAX=4

SQLITE_PATH=./geruest.db
SQLITE_BUSY_TIMEOUT_MS=5000
SQLITE_DB_EXECUTOR_THREADS=1

# PostgreSQL (only when DATABASE_BACKEND=postgres)
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_DB=app
POSTGRES_USER=app
POSTGRES_PASSWORD=secret
POSTGRES_SSLMODE=prefer
POSTGRES_CONNECT_TIMEOUT=5
POSTGRES_STATEMENT_TIMEOUT_MS=30000
# libpq pipeline batch size per worker (1 = pipeline off, max 256)
POSTGRES_PIPELINE_MAX_BATCH=8
```

Selection precedence:

1. Code setters (`setDatabaseBackend`, `setDatabasePoolSize`, `setSqliteExecutorThreadCount`, `configurePostgres`, `configureSqlite`) — each field tracks whether it was set; setters override env/.env for that field.
2. `DATABASE_BACKEND` and other keys from environment / `.env` (via `ConfigLoader`).
3. Built-in defaults (`DATABASE_BACKEND` default `none`, pool size `4`, one SQLite executor thread, etc.).

If `DATABASE_BACKEND` selects Postgres or SQLite but that backend was **not** compiled in (`GERUEST_HAS_LIBPQ=0` or `GERUEST_HAS_SQLITE=0`), `initializeDatabaseFromConfig()` throws and startup aborts.

If the backend **is** compiled in but **required connection settings are missing** (`POSTGRES_DB` or `POSTGRES_USER` empty for Postgres; `SQLITE_PATH` empty for SQLite), Geruest logs an error and leaves the shared database client unset. Then `request.database()` is null until you fix configuration — no exception in that path.

## Runtime Configuration (Code API)

`#include "Geruest.hpp"` pulls in `database/DatabaseClient.hpp` (types in namespace `geruest::db`). `configurePostgres` / `configureSqlite` are declared only when the matching `GERUEST_HAS_*` compile definition is `1`.

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

server.loadConfig(".env"); // fills any fields not already locked by setters/configure*
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
pg.connectTimeoutSeconds = 5;
pg.statementTimeoutMs = 30000;
pg.maxPipelineBatch = 8;  // 1 disables pipelining
// Optional tuning (not loaded from .env today):
// pg.tcpKeepalives, pg.keepalivesIdleSeconds, ...
server.configurePostgres(pg);
#endif
```

## Route Function Choice

- `addRoute(path, handler)`:
  - Handler returns `HTTPResponse`
  - No `co_await`
  - Best for static, auth checks, simple JSON
- `addRouteAsync(path, handler)`:
  - Handler returns `geruest::AsyncResponse`
  - Supports `co_await`
  - Use for DB access (`queryAsync`, `executeAsync`)

## Route Example (DB)

Use `addRouteAsync` for DB work. Access DB via `request.database()`.

```cpp
server.addRouteAsync("/users", [](const geruest::HTTPRequest& request) -> geruest::AsyncResponse {
    geruest::HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");

    auto db = request.database();
    if (!db) {
        response.setBody(R"({"error":"database not configured"})");
        co_return response;
    }

    auto json = co_await db->queryJsonAsync(
        "SELECT id, name FROM users WHERE name = ?",
        {std::string("alice")}
    );
    response.setBody(json.toString());
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

- Always pass values in the `params` vector (`std::vector<geruest::db::BindValue>`).
- Do not concatenate user input into SQL strings.
- `BindValue` is `std::variant<std::nullptr_t, std::int64_t, double, std::string>` — supported values: `nullptr`, integer, floating-point, and string.

## Query and execute results

- `queryAsync` returns `geruest::db::QueryResult`:
  - `columnNames` — names in column order (empty for statements with no result set).
  - `rows` — each `QueryRow` has `columns` as strings (SQLite `NULL` becomes empty string).
  - `affectedRows` — rows changed / command tag where applicable; `executeAsync` returns this same count as `std::uint64_t`.
- `queryJsonAsync` runs the same query and returns `geruest::JSONParser` via `geruest::db::toJSONParser(result)`.
- `toJSONParser(const QueryResult&)` builds `{"rows":[{...}, ...], "affectedRows":N}` — each row object uses `columnNames` as keys; if names are empty, keys are `"0"`, `"1"`, …
- `DatabaseClient::backend()` returns `geruest::db::Backend` (`None`, `Postgres`, `Sqlite`) for the concrete client implementation.

## SQLite Notes

- SQLite itself is synchronous.
- Geruest runs SQLite work on dedicated DB executor threads.
- This keeps HTTP worker threads non-blocking.
- Concurrent write order is not guaranteed; design tests accordingly.

## PostgreSQL Notes

- Uses libpq when built with `GERUEST_ENABLE_POSTGRESQL=ON` and `find_package(PostgreSQL)` succeeds.
- Connection string includes `connect_timeout` from `POSTGRES_CONNECT_TIMEOUT` / `PostgresConfig::connectTimeoutSeconds`.
- Each pooled connection sets `statement_timeout` from `POSTGRES_STATEMENT_TIMEOUT_MS` / `PostgresConfig::statementTimeoutMs`.
- Workers can batch multiple queued statements with libpq pipelining; batch size is capped by `POSTGRES_PIPELINE_MAX_BATCH` (clamped to 1–256). Use `1` to disable pipelining.
- TCP keepalive fields on `PostgresConfig` default to on; tune them only from C++ (not currently read from `.env`).

## Troubleshooting

- **`request.database()` is null**: `DATABASE_BACKEND` is `none`, the library was built without that backend, or Postgres/SQLite was selected but mandatory settings were missing (see logs: empty `POSTGRES_DB`/`POSTGRES_USER`, or empty `SQLITE_PATH`).
- **Startup throws “built without PostgreSQL/SQLite support”**: enable the backend in CMake and install `libpq-dev` / `libsqlite3-dev` so `GERUEST_HAS_LIBPQ` / `GERUEST_HAS_SQLITE` become `1`.
- **SQLite unit tests skipped or missing**: build the library with `-DGERUEST_ENABLE_SQLITE=ON` and see `src/unitTests/README.md` / Postgres docker notes in `src/unitTests/CMakeLists.txt` for integration tests.
