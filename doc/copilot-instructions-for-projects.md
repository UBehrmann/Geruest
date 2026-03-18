# Geruest Project — AI Instructions

C++17 web framework. Namespace: `geruest`. Header: `<Geruest.hpp>`.

## Server Setup

```cpp
#include <Geruest.hpp>
using namespace geruest;

Geruest server;
server.setPort(8080);
server.setHostname("localhost");
server.addRoot("website");                           // static file serving
server.setAvailableLanguages({"en", "de"});           // first = default
server.setMergeAssets(true);                          // CSS/JS bundling
server.setWebPConversion(true);                       // auto PNG/JPG → WebP
server.setWebPQuality(75.0f);                         // 0-100 (default 75)
server.enableDevMode();                               // debug logs, no cache, keep comments
server.setWorkerThreadCount(16);                      // default: CPU cores × 2
server.setMaxQueueSize(500);                          // connection queue limit
server.setLogLevel(LogLevel::Warning);                // None|Error|Warning|Info|Debug
server.loadConfig(".env");                            // .env + env vars (only unset values)
server.init();
server.start();
// server.stop();  server.isRunning();
```

## Routes

Signature: `using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;`

```cpp
server.addRoute("/api/data", [](const HTTPRequest& req) {
    HTTPResponse res("200 OK");
    res.setHeader("Content-Type", "application/json");
    res.setBody(R"({"ok":true})");
    return res;
});
```

Wildcards: `/api/*`, `/users/*/profile`. Exact routes = O(1), wildcards = O(n).

## HTTPRequest

```cpp
req.getMethod()            // "GET", "POST"
req.getPathString()        // "/api/users"
req.getPath(0)             // path segment at index
req.getBody()              // request body
req.getParam("q")          // query param (?q=value)
req.hasParam("q")
req.getHeader("Content-Type")
req.hasHeader("Authorization")
req.getClientIP()
req.getOrigin()            // Origin header value
req.getRawRequest()        // full raw HTTP request
req.getRawRequestLine()    // first line (e.g. "GET /path HTTP/1.1")
req.getServerRoot()        // server root path
urlDecode(str)             // free function: URL-decode a string
```

## HTTPResponse

```cpp
HTTPResponse res("200 OK");
res.setHeader("key", "value");
res.addHeader("Set-Cookie", "a=b");  // allows duplicate keys
res.setBody("content");              // auto Content-Length
res.toString();                      // full HTTP response string
```

Pre-built (all accept optional `const HTTPRequest* request` param):
`responseOK()`, `responseCreated()`, `responseAccepted()`, `responseNonAuthoritative()`, `responseNoContent()`, `responseResetContent()`, `responsePartialContent()`, `responseBadRequest()`, `responseAuthRequired()`, `responseUnauthorizedBasicAuth(realm)`, `responseForbidden()`, `responseNotFound()`, `responseMethodNotAllowed()`, `responseConflict()`, `responseInternalServerError()`

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
json.getArrayOfJSON("items");
json.getJSONArray();       // root-level JSON array

// Build
JSONParser j;
j.setString("k","v"); j.setInt("n",1); j.setBool("b",true);
j.setShort("s",1); j.setFloat("f",1.0f); j.setDouble("d",1.0);
j.setLong("l",1L); j.setLongLong("ll",1LL); j.setLongDouble("ld",1.0L);
j.setJSON("obj", otherJson);
j.setStringArray("k", vec); j.setIntArray("k", vec); // etc. for all types
j.setArrayOfJSON("k", vec); j.setJSONArray(vec);
j.addArrayOfJSON("k", vec); j.addJSONToArray(single);
j.toString();              // object → JSON string
j.arrayToString();         // array → JSON string

// Utilities
j.getKeys(); j.hasKey("k"); j.removeKey("k");

// File I/O
JSONParser* p = getJSONFromFile("f.json");            // caller must delete
auto up = getJSONFromFileSafe("f.json");              // returns unique_ptr
saveJSONToFile(json, "out.json");
saveArrayJSONToFile(jsonArr, "out.json");
```

## ConfigLoader

```cpp
#include <config/ConfigLoader.hpp>
using namespace geruest;

ConfigLoader::loadEnvFile(".env");                    // returns bool
ConfigLoader::get("KEY", "default");                  // string
ConfigLoader::getInt("PORT", 8080);
ConfigLoader::getFloat("QUALITY", 75.0f);
ConfigLoader::getBool("FLAG", false);                 // true/1/yes/on → true
ConfigLoader::getSizeT("COUNT", 0);
ConfigLoader::has("KEY");                             // bool
ConfigLoader::clear();
```

Priority: .env file > environment variables > default value.

## Auth

```cpp
server.setBasicAuthEnabled(true);
server.addBasicAuthUser("admin", "password");              // hashed internally (SHA-256)
server.addBasicAuthUserHashed("admin", "64charhexhash");   // pre-hashed
server.addProtectedPage("/admin");
server.removeBasicAuthUser("admin");                       // returns bool
server.removeProtectedPage("/admin");                      // returns bool
server.clearBasicAuthUsers();
server.clearProtectedPages();
std::string hash = Geruest::hashPassword("password");
```

## Email (requires libcurl, `GERUEST_HAS_CURL`)

```cpp
server.initEmail("smtp.gmail.com", 587, "user", "pass", "from@x.com", true /*TLS*/);
server.setEmailMinInterval(60);        // seconds between emails per IP
server.setEmailMaxPerIP(10);           // max emails per IP in tracking window
server.setEmailTrackingDuration(3600); // tracking window in seconds
server.setEmailMaxQueueSize(1000);

auto& sender = EmailSender::getInstance();
sender.enqueueEmail("to@x.com", "Subject", "Body", clientIP); // returns bool
std::string safe = EmailSender::sanitizeHeaderValue(userInput); // prevent SMTP injection

// Monitoring
sender.getEmailsSent(); sender.getEmailsRejected(); sender.getQueueSize();
sender.clearIPTracking();
sender.stop();
```

## Logging

```cpp
server.setLogLevel(LogLevel::Debug); // None, Error, Warning, Info, Debug
LogLevel lvl = server.getLogLevel();
```

Framework logging functions: `sendToLogger()`, `sendToLoggerError()`, `sendToLoggerAPI()`, `sendToLoggerPages()`

## WebP Conversion

```cpp
server.setWebPConversion(true);  // or server.enableWebPConversion();
server.setWebPQuality(85.0f);   // 0-100

// Direct usage (static)
WebPConverter::convertImage(srcPath, outPath, cacheOnly, quality);
WebPConverter::extractImagePathsFromHtml(html);
WebPConverter::replaceImageReferencesWithWebP(html);
WebPConverter::isConvertibleImage(path);
WebPConverter::getWebPPath(srcPath);
WebPConverter::hasInCache(path); WebPConverter::getFromCache(path);
```

## Templates

HTML components: `{components/header.html}` — path relative to website root, nested supported.
Translations: `[assets/translations/nav.json:home]` — auto language selection.

## Asset Merging

`server.setMergeAssets(true)` — scans HTML for `<link>` and `<script>`, merges into single files per page. Path normalization always runs (adds leading `/`, strips `/assets/css|js/` prefix).

## FileManagement

```cpp
#include <FileManagement/FileManagement.hpp>
FileManagement::createFolder(path);    // bool
FileManagement::fileExists(path);      // bool
FileManagement::createFile(path);      // bool
FileManagement::saveFile(path, content); // bool
FileManagement::deleteFile(path);
FileManagement::deleteFolder(path);
FileManagement::isOlderThan(path, hours); // bool
```

## ContentBuilder

```cpp
ContentBuilder builder(inputPath, serverData);
builder.file();        // get built content string
builder.size();        // content size
builder.sizeString();  // size as string
ContentBuilder::removeCommentsFromString(content, "html"); // "css", "js", "html"
ContentBuilder::loadFile(path); // load file as string
```

## Version

```cpp
#include <Version.hpp>
geruest::getVersion();       // e.g. "1.2.3"
geruest::getVersionMajor();  geruest::getVersionMinor();  geruest::getVersionPatch();
```

## CORS

Manual headers on each response:
```cpp
res.setHeader("Access-Control-Allow-Origin", "*");
res.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
```

## Website Structure

```
website/
├── html/           # pages (templates with {components} and [translations])
├── components/     # reusable {includes}
├── assets/
│   ├── css/  js/  images/
│   └── translations/   # JSON per feature, keyed by language
└── files_maps/     # css_file_map.json, js_file_map.json (bundling config)
```

## Build (CMake)

```cmake
cmake_minimum_required(VERSION 3.17)
set(CMAKE_CXX_STANDARD 17)
find_package(Geruest REQUIRED)
target_link_libraries(myapp PRIVATE Geruest::Geruest)
```

## Graceful Shutdown

```cpp
std::unique_ptr<Geruest> server;
std::signal(SIGINT, [](int) { if (server) server->stop(); });
```

## .env Keys

Server: `PORT`, `HOSTNAME`, `WORKER_THREADS`, `MAX_QUEUE_SIZE`, `LOG_LEVEL`, `DEV_MODE`, `MERGE_ASSETS`, `WEBP_CONVERSION`, `WEBP_QUALITY`
Email: `SMTP_SERVER`, `SMTP_PORT`, `SMTP_USERNAME`, `SMTP_PASSWORD`, `SMTP_FROM_ADDRESS`, `SMTP_USE_TLS`, `EMAIL_MIN_INTERVAL`, `EMAIL_MAX_PER_IP`, `EMAIL_TRACKING_DURATION`, `EMAIL_MAX_QUEUE_SIZE`
