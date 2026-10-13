# Status Endpoint (`/status`)

`enableStatus(token)` registers `GET /status`, a JSON snapshot of server health, traffic, errors, latency, process and host resources. Use it for uptime checks, dashboards, and alerting.

```cpp
server.enableStatus("my-secret-token");   // before start()
server.setStatusPersistencePath("/var/lib/myapp/status.json");  // optional
```

## Access

- Production (dev mode off): send `Authorization: Bearer <token>` **or** `?token=<token>`. Anything else returns `401 {"error":"Unauthorized"}` and increments `errors.status_auth_failures`.
- Dev mode: no token required.
- Prefer the header. Query strings end up in proxy and access logs.
- Responses carry `Cache-Control: no-store`.
- Requests to `/status` itself are **not** counted in any request, error, or latency metric.

## Example response

```json
{
  "health": "degraded",
  "health_reasons": ["5xx rate 7% (5m)"],
  "version": "0.15.0",
  "timestamp": "2026-10-13T20:00:00Z",
  "uptime_seconds": 86400,
  "uptime_hours_total": 1250.4,
  "requests":  { "total": 912345, "active": 3, "last_5m": 1500, "last_hour": 18000,
                 "avg_per_hour": 729.6, "status_2xx": 850000, "status_3xx": 40000 },
  "errors":    { "total": 22345, "client_4xx": 21000, "server_5xx": 1300, "internal": 45,
                 "last_5m_4xx": 20, "last_5m_5xx": 105, "last_5m_int": 0,
                 "last_hour_4xx": 240, "last_hour_5xx": 130, "last_hour_int": 0,
                 "avg_per_hour_4xx": 16.8, "avg_per_hour_5xx": 1.04, "avg_per_hour_int": 0.04,
                 "server_error_rate_5m": 7.0, "status_auth_failures": 2 },
  "queue":     { "current_size": 12, "max_size": 500, "rejections_total": 0, "rejections_last_5m": 0,
                 "avg_fill_percent_5m": 2.1, "avg_fill_percent_hour": 1.8 },
  "io":        { "connections_total": 300000, "accept_errors_total": 0, "accept_emfile_total": 0,
                 "framing_rejects_total": 4, "write_failures_total": 37, "file_open_failures_total": 0 },
  "websocket": { "active": 5, "upgrades_total": 812 },
  "latency_ms":      { "p50": 1.2, "p95": 8.7, "p99": 41.0 },
  "latency_ms_hour": { "p50": 1.1, "p95": 7.9, "p99": 35.5 },
  "process":   { "rss_mb": 84, "threads": 18, "open_fds": 61, "max_fds": 1024, "fd_percent_used": 5.96 },
  "email":     { "queue_size": 0, "sent_total": 120, "failed_total": 1, "rejected_total": 3 },
  "system": {
    "memory": { "total_mb": 7820, "used_mb": 3100, "free_mb": 4720, "percent_used": 39.6 },
    "cpu":    { "count": 4, "load_1m": 0.42, "load_5m": 0.35, "load_15m": 0.30 },
    "disk":   { "path": "/srv/site/website", "total_gb": 100, "used_gb": 40, "free_gb": 60, "percent_used": 40.0 },
    "cgroup_memory": { "limit_mb": 2048, "used_mb": 256, "free_mb": 1792, "percent_used": 12.5 },
    "cgroup_cpu":    { "allocated_cores": 2.0, "usage_percent": 3.5 }
  }
}
```

## Time windows

Per-minute buckets cover the last 60 minutes.

- **`_5m`** fields cover the current (partial) minute plus the 4 before it. That is 4 to 5 minutes of data, which is the right window for alerting.
- **`_hour`** fields cover the last 60 minute buckets.
- **`total` / `_total`** fields are lifetime counters. They are persisted across restarts (see [Persistence](#persistence)).
- **`avg_per_hour*`** = lifetime total ÷ lifetime uptime hours. During the first hour of lifetime uptime it equals the total.

## Fields

### Top level

| Field | Meaning |
|---|---|
| `health` | `"ok"`, `"degraded"` or `"overloaded"`. See [Health](#health). |
| `health_reasons` | Human-readable list of every rule that fired; empty when `ok`. |
| `version` | Geruest framework version. |
| `timestamp` | Snapshot time, ISO 8601 UTC. |
| `uptime_seconds` | Seconds since this process entered `start()`. |
| `uptime_hours_total` | Cumulative uptime across restarts (persisted). |

### `requests`

| Field | Meaning |
|---|---|
| `total` | HTTP requests received (lifetime). WebSocket upgrade requests count once. |
| `active` | Requests being handled right now. Open WebSocket sessions are **not** included (see `websocket.active`). |
| `last_5m`, `last_hour` | Requests in the window. |
| `avg_per_hour` | Lifetime average. |
| `status_2xx`, `status_3xx` | Responses by status class (lifetime). 4xx/5xx are in `errors`. |

Status classes come from the status line actually written to the socket, so every response path is counted: routes, static files, 404 pages, gates, basic auth, CORS denials and WebSocket handshake errors. A request that got no response (client vanished, connection dropped) has no status class.

### `errors`

| Field | Meaning |
|---|---|
| `client_4xx`, `server_5xx` | Responses with a 4xx / 5xx status (lifetime). |
| `internal` | Exceptions that escaped a handler without a response, plus exceptions thrown inside WebSocket handlers. |
| `total` | `client_4xx + server_5xx + internal`. |
| `last_5m_*`, `last_hour_*` | Same three counters per window (`int` = internal). |
| `avg_per_hour_*` | Lifetime averages. |
| `server_error_rate_5m` | `(5xx + internal) / requests × 100` over the 5-minute window. |
| `status_auth_failures` | Rejected `/status` calls (wrong or missing token). A rising value means someone is probing the token. |

### `queue`

The "queue" is the **concurrent session cap** set by `setMaxQueueSize` / `MAX_QUEUE_SIZE`. It is not a kernel backlog.

| Field | Meaning |
|---|---|
| `current_size` | Open client connections right now (keep-alive and WebSocket included). |
| `max_size` | The session cap. |
| `rejections_total`, `rejections_last_5m` | Connections refused with `503` because the cap was reached. |
| `avg_fill_percent_5m`, `avg_fill_percent_hour` | `current_size / max_size`, sampled every second and averaged (time-weighted). |

### `io`

| Field | Meaning |
|---|---|
| `connections_total` | TCP connections accepted (including ones rejected at the cap). |
| `accept_errors_total` | `accept()` failures. |
| `accept_emfile_total` | Subset caused by file-descriptor exhaustion. Raise `ulimit -n` (compare `process.open_fds` / `max_fds`). |
| `framing_rejects_total` | Requests refused before routing: headers too large, invalid `Content-Length`, body over the limit. |
| `write_failures_total` | Responses that could not be fully written (client disconnected mid-response). A few are normal. A spike usually means slow clients or a proxy timeout. |
| `file_open_failures_total` | Static files that exist but could not be opened (answered with 503). |

### `websocket`

| Field | Meaning |
|---|---|
| `active` | Open WebSocket sessions. |
| `upgrades_total` | Successful handshakes (lifetime). |

### `latency_ms` and `latency_ms_hour`

`p50`, `p95` and `p99` in milliseconds, measured from request parsed to response fully written, over the 5-minute and 1-hour windows. WebSocket sessions and `/status` are excluded.

Values come from a histogram whose bucket bounds grow by √2 starting at 50 µs, interpolated within the bucket. Expect roughly ±20% accuracy. This is plenty for trends and alerts, but it is not suitable for benchmarking. Memory stays constant regardless of traffic. `0` means no samples.

### `process`

| Field | Meaning |
|---|---|
| `rss_mb` | Resident memory of the server process. |
| `threads` | OS threads in the process. |
| `open_fds`, `max_fds` | Open file descriptors vs the soft `RLIMIT_NOFILE` (`0` = unlimited). |
| `fd_percent_used` | `open_fds / max_fds × 100`. |

### `email` (only when `initEmail()` was called)

| Field | Meaning |
|---|---|
| `queue_size` | Emails waiting to be sent. |
| `sent_total` | Delivered to the SMTP server since start (not persisted). |
| `failed_total` | Dropped after 3 failed SMTP attempts. Alert on any increase. |
| `rejected_total` | Refused by spam protection (rate limit per IP). |

### `system`

| Field | Meaning |
|---|---|
| `memory.*` | Host memory (`/proc/meminfo`, `used = total − available`). |
| `cpu.count` | Logical cores. |
| `cpu.load_1m/5m/15m` | Host load average. `load / count × 100` ≈ CPU %. |
| `disk.*` | Filesystem holding the website root (`addRoot`), falling back to `/`. `path` shows which one was used. |
| `cgroup_memory.*` | Container memory limit and usage. Only present when a limit is set (Docker `--memory`). |
| `cgroup_cpu.allocated_cores` | CPU quota (Docker `--cpus`). Only present when a quota is set. |
| `cgroup_cpu.usage_percent` | Share of that quota used, sampled every 5 seconds in the background. The field is missing for the first second after start. |

## Health

Rules use the 5-minute window. The worst rule wins, and every rule that fires adds a line to `health_reasons`.

| Rule | `degraded` | `overloaded` |
|---|---|---|
| Connections rejected at the session cap | | any |
| Average queue fill | ≥ 50% | ≥ 80% |
| Memory used (cgroup limit if set, else host) | ≥ 90% | ≥ 95% |
| File descriptors used | ≥ 80% | ≥ 90% |
| Server error rate (with ≥ 20 requests in the window) | ≥ 5% | |
| p99 latency | ≥ 2000 ms | |
| Disk used | ≥ 95% | |

`overloaded` means the server is at a capacity limit and is refusing or about to refuse work. `degraded` means it is serving, but something needs attention.

## Persistence

Lifetime counters, lifetime uptime, and the 60 minute buckets (including latency histograms) are saved as JSON:

- Saved every hour and on clean shutdown.
- The file is `geruest-status-state.json` in the working directory by default. Change it with `setStatusPersistencePath()`.
- Files from older versions (schema 1) still load. Unknown or corrupt files are ignored (counters start at zero) and an error is logged.
- Point the path at a persistent volume in Docker, otherwise counters reset with every container.

## Monitoring examples

```bash
# Uptime check
curl -fsS -H "Authorization: Bearer $TOKEN" https://example.com/status | jq -e '.health != "overloaded"'

# Why is it not ok?
curl -s -H "Authorization: Bearer $TOKEN" https://example.com/status | jq '.health, .health_reasons'
```

Typical alerts:

- `health == "overloaded"` for 2 or more checks.
- `errors.server_error_rate_5m > 5`.
- `latency_ms.p99` above your SLO.
- `email.failed_total` increased.
- `io.accept_emfile_total` increased.
- `errors.status_auth_failures` increasing quickly.
