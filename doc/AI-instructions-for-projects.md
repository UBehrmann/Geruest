# Geruest Project — AI Instructions

C++20 web framework. Namespace: `geruest`. Header: `<Geruest.hpp>`.
Version: 0.7.8. Build system: CMake 3.10+. **Core HTTP server** depends on **Boost** (Asio + Boost.System, 1.75+). Optional: libcurl (email), libwebp (WebP).

## Quick Summary

| Concept | Key class / function |
|---|---|
| Server | `Geruest` (main class) |
| Request | `HTTPRequest` |
| Response | `HTTPResponse` + pre-built helpers |
| JSON | `JSONParser` (string-based, no `std::any`) |
| Config / .env | `ConfigLoader` |
| Auth | Basic Auth via `Geruest` methods |
| Templates | `ContentBuilder` ({components}, [translations]) |
| Status endpoint | `server.enableStatus(token)` → `GET /status` |
| Email (optional) | `EmailSender` — requires `GERUEST_HAS_CURL` |
| WebP (optional) | `WebPConverter` — requires `GERUEST_HAS_WEBP` |

---

## Server Setup

```cpp
#include <Geruest.hpp>
using namespace geruest;

Geruest server;
server.setPort(8080);
server.setHostname("localhost");
server.addRoot("website");                           // static file serving root
server.setAvailableLanguages({"en", "de"});           // first = default
server.setMergeAssets(true);                          // CSS/JS bundling
server.setWebPConversion(true);                       // auto PNG/JPG → WebP
server.setWebPQuality(75.0f);                         // 0-100 (default 75)
server.enableDevMode();                               // debug logs, no cache, keep comments
server.setWorkerThreadCount(16);                      // default: CPU cores × 2
server.setMaxQueueSize(500);                          // max concurrent TCP sessions (see /status `queue`)
server.setLogLevel(LogLevel::Warning);                // None|Error|Warning|Info|Debug
server.loadConfig(".env");                            // .env + env vars (only unset values)
server.enableStatus("my-secret-token");               // activate /status endpoint
server.init();
server.start();
// server.stop();  server.isRunning();

// Graceful shutdown
std::signal(SIGINT, [](int) { if (server) server->stop(); });
```

---

## Routes

```cpp
server.addRoute("/api/data", [](const HTTPRequest& req) {
    HTTPResponse res("200 OK");
    res.setHeader("Content-Type", "application/json");
    res.setBody(R"({"ok":true})");
    return res;
});
```

Wildcards: `/api/*`, `/users/*/profile`. Exact routes = O(1), wildcards = O(n).

---

## HTTPRequest

```cpp
req.getMethod()            // "GET", "POST", ...
req.getPathString()        // "/api/users"
req.getPath(0)             // path segment at index 0
req.getBody()              // raw request body
req.getParam("q")          // query param (?q=value)
req.hasParam("q")          // bool
req.getHeader("Content-Type")
req.hasHeader("Authorization")
req.getClientIP()
req.getOrigin()
req.getRawRequest()        // full raw HTTP request
req.getRawRequestLine()    // e.g. "GET /path HTTP/1.1"
req.getServerRoot()
urlDecode(str)             // free function
```

---

## HTTPResponse

```cpp
HTTPResponse res("200 OK");
res.setHeader("key", "value");
res.addHeader("Set-Cookie", "a=b");  // addHeader allows duplicate keys
res.setBody("content");              // auto Content-Length
res.toString();                      // full HTTP response string
```

Pre-built helpers (all accept optional `const HTTPRequest*`):

```cpp
responseOK()                         // 200
responseCreated()                    // 201
responseAccepted()                   // 202
responseNonAuthoritative()           // 203
responseNoContent()                  // 204
responseResetContent()               // 205
responsePartialContent()             // 206
responseBadRequest()                 // 400
responseAuthRequired()               // 401
responseUnauthorizedBasicAuth(realm) // 401 + WWW-Authenticate
responseForbidden()                  // 403
responseNotFound()                   // 404
responseMethodNotAllowed()           // 405
responseConflict()                   // 409
responseInternalServerError()        // 500
```

---

## Status Endpoint

```cpp
server.enableStatus("my-secret-token");
```

Registers `GET /status`. In production (dev mode off), requires:
- `Authorization: Bearer <token>`, **or** `?token=<token>` query param.
- Returns `401 {"error":"Unauthorized"}` on failure.

Response: `200 OK`, `Content-Type: application/json`, `Cache-Control: no-store`.

```json
{
  "health": "ok",
  "version": "0.7.8",
  "timestamp": "2025-07-03T12:00:00Z",
  "uptime_seconds": 3600,
  "requests":   { "total": 0, "active": 0, "last_hour": 0, "avg_per_hour": 0 },
  "errors":     { "total": 0, "client_4xx": 0, "server_5xx": 0, "internal": 0,
                  "last_hour_4xx": 0, "last_hour_5xx": 0, "last_hour_int": 0,
                  "avg_per_hour_4xx": 0, "avg_per_hour_5xx": 0, "avg_per_hour_int": 0 },
  "queue":      { "current_size": 0, "max_size": 500, "rejections_total": 0,
                  "avg_fill_percent_hour": 0.0, "avg_fill_percent_per_hour": 0.0 },
  "latency_ms": { "p50": 0.0, "p95": 0.0, "p99": 0.0 },
  "system": {
    "memory": { "total_mb": 0, "used_mb": 0, "free_mb": 0, "percent_used": 0.0 },
    "cpu":    { "count": 0, "load_1m": 0.0, "load_5m": 0.0, "load_15m": 0.0 },
    "disk":   { "total_gb": 0, "used_gb": 0, "free_gb": 0, "percent_used": 0.0 },
    "cgroup_memory": { "limit_mb": 0, "used_mb": 0, "free_mb": 0, "percent_used": 0.0 },
    "cgroup_cpu":    { "allocated_cores": 0.0, "usage_percent": 0.0 }
  }
}
```

`queue.current_size` / `queue.max_size` are **active client sessions** vs **`setMaxQueueSize` / `MAX_QUEUE_SIZE`** (not a kernel listen backlog). `avg_fill_percent_*` is average **session utilization** (0–100%) sampled over time. Rejections occur when a new connection arrives at the session cap.

`cgroup_memory` and `cgroup_cpu` are only present when cgroup limits are detected (Linux containers).

| `health` | Condition |
|---|---|
| `"ok"` | session utilization below 50% (rolling) and requests below 500/hour |
| `"degraded"` | session utilization ≥ 50% **or** requests ≥ 500/hour |
| `"overloaded"` | session utilization ≥ 80% **or** requests ≥ 1000/hour |

---

## JSONParser

String-based storage — all values stored as strings, converted on get/set.

```cpp
// Parse
JSONParser json(R"({"name":"Jo","age":30})");
json.getString("name");  json.getInt("age");  json.getBool("active");
json.getShort("s"); json.getFloat("f"); json.getDouble("d");
json.getLong("l"); json.getLongLong("ll"); json.getLongDouble("ld");
json.getObject("nested");
json.getStringArray("tags"); json.getIntArray("ids"); json.getBoolArray("flags");
json.getShortArray("sa"); json.getLongArray("la"); json.getLongLongArray("lla");
json.getFloatArray("fa"); json.getDoubleArray("da"); json.getLongDoubleArray("lda");
json.getArrayOfJSON("items");  json.getJSONArray();

// Build
JSONParser j;
j.setString("k","v"); j.setInt("n",1); j.setBool("b",true);
j.setShort("s",1); j.setFloat("f",1.0f); j.setDouble("d",1.0);
j.setLong("l",1L); j.setLongLong("ll",1LL); j.setLongDouble("ld",1.0L);
j.setJSON("obj", otherJson);
j.setStringArray("k", vec); j.setIntArray("k", vec); // etc. for all types
j.setArrayOfJSON("k", vec); j.setJSONArray(vec);
j.addArrayOfJSON("k", vec); j.addJSONToArray(single);
j.toString();        // object → JSON string
j.arrayToString();   // array → JSON string

// Utilities
j.getKeys(); j.hasKey("k"); j.removeKey("k");

// File I/O
auto up = getJSONFromFileSafe("f.json");   // unique_ptr<JSONParser> (preferred)
JSONParser* p = getJSONFromFile("f.json"); // caller must delete
saveJSONToFile(json, "out.json");
saveArrayJSONToFile(jsonArr, "out.json");
```

Always call the typed getter matching how the value was set — all values are stored as strings internally.

---

## ConfigLoader

```cpp
#include <config/ConfigLoader.hpp>
ConfigLoader::loadEnvFile(".env");          // returns bool
ConfigLoader::get("KEY", "default");        // string
ConfigLoader::getInt("PORT", 8080);
ConfigLoader::getFloat("QUALITY", 75.0f);
ConfigLoader::getBool("FLAG", false);       // true/1/yes/on → true
ConfigLoader::getSizeT("COUNT", 0);
ConfigLoader::has("KEY");
ConfigLoader::clear();
```

Priority: `.env` file > environment variables > default value.

---

## Auth (Basic Auth)

```cpp
server.setBasicAuth(true);
server.addBasicAuthUser("admin", "password");            // hashed internally (SHA-256)
server.addBasicAuthUserHashed("admin", "64charhex");     // pre-hashed
server.addProtectedPage("/admin");
server.removeBasicAuthUser("admin");                     // returns bool
server.removeProtectedPage("/admin");                    // returns bool
server.clearBasicAuthUsers();
server.clearProtectedPages();
std::string hash = Geruest::hashPassword("password");
```

---

## Logging

```cpp
server.setLogLevel(LogLevel::Debug); // None, Error, Warning, Info, Debug
sendToLogger("message");        // general info
sendToLoggerError("message");   // errors / warnings
sendToLoggerAPI("message");     // API request logging
sendToLoggerPages("message");   // page request logging
```

---

## Email (requires libcurl, `GERUEST_HAS_CURL`)

```cpp
server.initEmail("smtp.gmail.com", 587, "user", "pass", "from@x.com", true /*TLS*/);
server.setEmailMinInterval(60);        // seconds between emails per IP
server.setEmailMaxPerIP(10);
server.setEmailTrackingDuration(3600);
server.setEmailMaxQueueSize(1000);

auto& sender = EmailSender::getInstance();
sender.enqueueEmail("to@x.com", "Subject", "Body", clientIP); // returns bool
std::string safe = EmailSender::sanitizeHeaderValue(userInput);
sender.getEmailsSent(); sender.getEmailsRejected(); sender.getQueueSize();
sender.clearIPTracking();  sender.stop();
```

---

## WebP Conversion (requires libwebp, `GERUEST_HAS_WEBP`)

```cpp
server.setWebPConversion(true);
server.setWebPQuality(85.0f);  // 0-100
WebPConverter::convertImage(srcPath, outPath, cacheOnly, quality);
WebPConverter::extractImagePathsFromHtml(html);
WebPConverter::replaceImageReferencesWithWebP(html);
WebPConverter::isConvertibleImage(path);
WebPConverter::getWebPPath(srcPath);
WebPConverter::hasInCache(path);  WebPConverter::getFromCache(path);
```

---

## Templates & ContentBuilder

HTML components: `{components/header.html}` — path relative to website root, nested supported.
Translations: `[assets/translations/nav.json:home]` — auto language selection.

```cpp
ContentBuilder builder(inputPath, serverData);
builder.file();        // built content string
builder.size();        // content size (bytes)
ContentBuilder::removeCommentsFromString(content, "html"); // "css", "js", "html"
ContentBuilder::loadFile(path);
```

Asset merging: `server.setMergeAssets(true)` — merges `<link>` and `<script>` tags into single files per page.

---

## FileManagement

```cpp
#include <FileManagement/FileManagement.hpp>
FileManagement::createFolder(path);       // bool
FileManagement::fileExists(path);         // bool
FileManagement::createFile(path);         // bool
FileManagement::saveFile(path, content);  // bool
FileManagement::deleteFile(path);
FileManagement::deleteFolder(path);
FileManagement::isOlderThan(path, hours); // bool
```

---

## CORS

```cpp
res.setHeader("Access-Control-Allow-Origin", "*");
res.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
```

---

## Website Structure

```
website/
├── html/           # pages (templates with {components} and [translations])
├── components/     # reusable {includes}
├── assets/
│   ├── css/  js/  images/
│   └── translations/   # JSON per feature, keyed by language code
├── configs/
│   └── restrictions.json
└── files_maps/     # css_file_map.json, js_file_map.json
```

---

## Build

**Consumer CMake:**
```cmake
cmake_minimum_required(VERSION 3.17)
set(CMAKE_CXX_STANDARD 20)
find_package(Boost 1.75 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(Geruest REQUIRED)
target_link_libraries(myapp PRIVATE Geruest::Geruest Boost::system Threads::Threads)
```

**Build library (Linux):** `sudo apt-get install -y libboost-system-dev` then `mkdir build && cd build && cmake .. && make -j$(nproc) && make install`

**Optional deps:** Email → `apt install libcurl4-openssl-dev` / `vcpkg install curl` | WebP → `apt install libwebp-dev` / `vcpkg install libwebp`

**Unit tests:** `cd src/unitTests/build && ctest --output-on-failure` (174 tests, 9 suites)

---

## Version

```cpp
#include <Version.hpp>
geruest::getVersion();       // e.g. "0.7.8"
geruest::getVersionMajor();  geruest::getVersionMinor();  geruest::getVersionPatch();
```

---

## .env Keys

```
PORT  HOSTNAME  WORKER_THREADS  MAX_QUEUE_SIZE  LOG_LEVEL  DEV_MODE  MERGE_ASSETS  WEBP_CONVERSION  WEBP_QUALITY
SMTP_SERVER  SMTP_PORT  SMTP_USERNAME  SMTP_PASSWORD  SMTP_FROM_ADDRESS  SMTP_USE_TLS
EMAIL_MIN_INTERVAL  EMAIL_MAX_PER_IP  EMAIL_TRACKING_DURATION  EMAIL_MAX_QUEUE_SIZE
```

Priority: `.env` file → environment variables → code defaults. `loadConfig()` only sets values not already set in code.
