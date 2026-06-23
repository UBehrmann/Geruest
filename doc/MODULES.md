# Geruest modules (v0.13+)

Geruest ships as optional static libraries plus one umbrella CMake target. Link only what you need.

## Targets

| CMake target | Role |
|--------------|------|
| `Geruest::Core` | HTTP server, routing, gates, static passthrough, metrics, `JSONParser`, `Security`, i18n URL routing |
| `Geruest::WebSocket` | RFC 6455 protocol, upgrade session, `addRouteWebSocket` implementations |
| `Geruest::Database` | `DatabaseClient`, `DbExecutor` (when built with PostgreSQL and/or SQLite) |
| `Geruest::Email` | `EmailSender`, SMTP config from `.env` (requires libcurl) |
| `Geruest::Obfuscation` | `JSObfuscator`, `JSObfuscatorScope`, `ObfuscationSettings` data |
| `Geruest::Assets` | `ContentBuilder`, asset merge, HTML/i18n pipeline, on-demand WebP |
| `Geruest::Geruest` | **INTERFACE** — links Core plus every module enabled at configure time (backward compatible) |

## Dependency graph

```
Geruest::Geruest (INTERFACE)
  ├── Geruest::Core
  ├── Geruest::Obfuscation  (optional)
  ├── Geruest::Assets       → Core, Obfuscation
  ├── Geruest::WebSocket    → Core
  ├── Geruest::Database     → Core
  └── Geruest::Email        → Core
```

## CMake options (defaults preserve full stack)

| Option | Default | Effect |
|--------|---------|--------|
| `GERUEST_ENABLE_WEBSOCKET` | ON | Build `Geruest::WebSocket` |
| `GERUEST_ENABLE_ASSETS` | ON | Build `Geruest::Assets` |
| `GERUEST_ENABLE_OBFUSCATION` | ON | Build `Geruest::Obfuscation` |
| `GERUEST_ENABLE_EMAIL` | ON | Build `Geruest::Email` (requires CURL) |
| `GERUEST_ENABLE_POSTGRESQL` | OFF | Add `Geruest::Database` with libpq |
| `GERUEST_ENABLE_SQLITE` | OFF | Add `Geruest::Database` with SQLite |

## Link lines

**Full stack (existing apps):**

```cmake
find_package(Geruest REQUIRED)
target_link_libraries(myapp PRIVATE Geruest::Geruest Boost::system Threads::Threads)
```

**API-only / minimal HTTP server:**

```cmake
target_link_libraries(myapp PRIVATE Geruest::Core Boost::system Threads::Threads)
```

**Core + WebSocket:**

```cmake
target_link_libraries(myapp PRIVATE Geruest::Core Geruest::WebSocket Boost::system Threads::Threads)
```

**Static site with merge and obfuscation:**

```cmake
target_link_libraries(myapp PRIVATE Geruest::Core Geruest::Assets Boost::system Threads::Threads)
# Assets already pulls Obfuscation
```

## Public headers

Installed under `include/geruest/`:

| Header | Module | Notes |
|--------|--------|-------|
| `geruest/Core.hpp` | Core | `Geruest`, HTTP types, routes, gates, config |
| `geruest/WebSocket.hpp` | WebSocket | WS types and limits |
| `geruest/Database.hpp` | Database | `setDatabaseBackend`, `DatabaseClient` |
| `geruest/Email.hpp` | Email | `EmailSender`, env SMTP helper |
| `geruest/Obfuscation.hpp` | Obfuscation | Obfuscation settings types |
| `geruest/Assets.hpp` | Assets | Merge / WebP-related builders |
| `geruest/Geruest.hpp` | Umbrella | Includes all module headers above |
| `Geruest.hpp` | Core | Legacy path; same main class |

`geruest/BuildConfig.hpp` (generated at configure time) defines `GERUEST_ENABLE_*` macros so headers match your build.

## Which APIs need which target?

| API area | Minimum link |
|----------|----------------|
| `addRoute`, gates, CORS, static files, `/status` | `Geruest::Core` |
| `addRouteWebSocket`, WS limits | `Geruest::WebSocket` (or umbrella) |
| `request.database()`, `configurePostgres` / `configureSqlite` | `Geruest::Database` |
| `initEmail`, `EmailSender` | `Geruest::Email` |
| `setObfuscationLevel`, exclusions, extern lists | Core stores settings; **obfuscation runs** when `Geruest::Obfuscation` + `Geruest::Assets` are linked |
| `setMergeAssets`, `enableWebPConversion`, HTML/CSS/JS pipeline | `Geruest::Assets` |

Without `Geruest::Assets`, text static files (`.html`, `.css`, `.js`) are served **raw** from disk. Without `Geruest::WebSocket`, upgrade requests are not handled. Without `Geruest::Email`, email methods are not linked (guard with `GERUEST_ENABLE_EMAIL` in `BuildConfig.hpp`).

## Internal module hooks

Optional features register at link time via `src/modules/ModuleHooks.hpp` (not public API): text content processing, WebSocket upgrade, merged-asset lookup, on-demand WebP, SMTP env application. Linking a module static library runs its registrar.

## Examples in this repo

| Example | Link target |
|---------|-------------|
| [exemples/minimal](../exemples/minimal/) | `Geruest` (umbrella) — can be changed to `Geruest::Core` for a smaller binary |
| [exemples/showcase](../exemples/showcase/) | `Geruest` (umbrella) |

See [GETTING_STARTED.md](GETTING_STARTED.md) for a Core-only snippet.
