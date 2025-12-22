# Data Classes Reference

This document provides detailed API documentation for Geruest's core data classes.

## Table of Contents

- [HTTPRequest](#httprequest)
- [HTTPResponse](#httpresponse)
- [JSONParser](#jsonparser)

---

## HTTPRequest

The `HTTPRequest` class parses and provides access to incoming HTTP request data.

### Constructor

```cpp
HTTPRequest(std::string rawRequest, std::string clientIP, std::string serverRootPath);
```

The constructor is called internally by the Handler. You receive a `const HTTPRequest&` in your route handlers.

### Methods

#### Request Information

| Method | Return Type | Description |
|--------|-------------|-------------|
| `getMethod()` | `std::string` | HTTP method (GET, POST, PUT, DELETE, etc.) |
| `getPathString()` | `std::string` | Full request path (e.g., `/api/users`) |
| `getPath(size_t index)` | `std::string` | Path segment at index (0-based) |
| `getRawRequest()` | `std::string` | Complete raw HTTP request |
| `getRawRequestLine()` | `std::string` | First line (e.g., `GET /api HTTP/1.1`) |
| `getClientIP()` | `std::string` | Client's IP address |
| `getOrigin()` | `std::string` | Origin header value |
| `getServerRoot()` | `std::string` | Server's root directory path |
| `getBody()` | `std::string` | Request body content |

#### Query Parameters

```cpp
// URL: /search?q=hello&page=2

// Check if parameter exists
bool hasQ = req.hasParam("q");        // true
bool hasSort = req.hasParam("sort");  // false

// Get parameter value
std::string query = req.getParam("q");    // "hello"
std::string page = req.getParam("page");  // "2"
std::string sort = req.getParam("sort");  // "" (empty if not found)
```

#### Headers

```cpp
// Check if header exists
bool hasAuth = req.hasHeader("Authorization");
bool hasCookie = req.hasHeader("Cookie");

// Get header value (case-insensitive)
std::string contentType = req.getHeader("Content-Type");
std::string auth = req.getHeader("Authorization");
std::string userAgent = req.getHeader("User-Agent");
```

### Usage Examples

#### Basic Route Handler

```cpp
server.addRoute("/api/users", [](const HTTPRequest& req) {
    std::string method = req.getMethod();
    std::string path = req.getPathString();
    std::string clientIP = req.getClientIP();
    
    std::cout << method << " " << path << " from " << clientIP << std::endl;
    
    HTTPResponse response("200 OK");
    response.setBody("OK");
    return response;
});
```

#### Handling Query Parameters

```cpp
server.addRoute("/search", [](const HTTPRequest& req) {
    if (!req.hasParam("q")) {
        return responseBadRequest();
    }
    
    std::string query = req.getParam("q");
    std::string page = req.hasParam("page") ? req.getParam("page") : "1";
    
    // Perform search...
    
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"query": ")" + query + R"(", "page": )" + page + "}");
    return response;
});
```

#### Processing POST Body

```cpp
server.addRoute("/api/data", [](const HTTPRequest& req) {
    if (req.getMethod() != "POST") {
        return responseMethodNotAllowed();
    }
    
    std::string body = req.getBody();
    std::string contentType = req.getHeader("Content-Type");
    
    if (contentType.find("application/json") != std::string::npos) {
        // Parse JSON body
        JSONParser json(body);
        std::string name = json.getString("name");
        // Process...
    }
    
    return responseCreated();
});
```

#### Extracting Path Segments

```cpp
server.addRoute("/users/*/posts/*", [](const HTTPRequest& req) {
    // Path: /users/123/posts/456
    std::string userId = req.getPath(1);   // "123"
    std::string postId = req.getPath(3);   // "456"
    
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"userId": ")" + userId + R"(", "postId": ")" + postId + R"("})");
    return response;
});
```

---

## HTTPResponse

The `HTTPResponse` class builds HTTP responses with headers and body.

### Constructor

```cpp
explicit HTTPResponse(const std::string& statusCode);
```

Status code should be in format `"CODE MESSAGE"` (e.g., `"200 OK"`, `"404 Not Found"`).

### Methods

#### Setting Headers

```cpp
// Set a header (replaces existing)
void setHeader(const std::string& key, const std::string& value);

// Add a header (allows duplicates, e.g., Set-Cookie)
void addHeader(const std::string& key, const std::string& value);
```

#### Setting Body

```cpp
// Set response body (automatically sets Content-Length)
void setBody(const std::string& responseBody);
```

#### Output

```cpp
// Convert to complete HTTP response string
std::string toString() const;
```

### Pre-built Response Functions

Geruest provides convenient functions for common responses:

#### Success Responses (2xx)

```cpp
HTTPResponse response = responseOK();           // 200 OK
HTTPResponse response = responseCreated();      // 201 Created
HTTPResponse response = responseAccepted();     // 202 Accepted
HTTPResponse response = responseNoContent();    // 204 No Content
```

#### Client Error Responses (4xx)

```cpp
HTTPResponse response = responseBadRequest();        // 400 Bad Request
HTTPResponse response = responseAuthRequired();      // 401 Unauthorized
HTTPResponse response = responseForbidden();         // 403 Forbidden
HTTPResponse response = responseNotFound();          // 404 Not Found
HTTPResponse response = responseMethodNotAllowed();  // 405 Method Not Allowed
HTTPResponse response = responseConflict();          // 409 Conflict
```

#### Server Error Responses (5xx)

```cpp
HTTPResponse response = responseInternalServerError();  // 500 Internal Server Error
```

#### Authentication Response

```cpp
// 401 with WWW-Authenticate header for Basic Auth
HTTPResponse response = responseUnauthorizedBasicAuth("Admin Area");
```

### Usage Examples

#### JSON Response

```cpp
server.addRoute("/api/status", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Cache-Control", "no-cache");
    response.setBody(R"({"status": "healthy", "uptime": 3600})");
    return response;
});
```

#### HTML Response

```cpp
server.addRoute("/page", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    response.setBody("<html><body><h1>Hello!</h1></body></html>");
    return response;
});
```

#### Setting Cookies

```cpp
server.addRoute("/login", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    
    // Use addHeader for multiple cookies
    response.addHeader("Set-Cookie", "session=abc123; HttpOnly; Path=/");
    response.addHeader("Set-Cookie", "user=john; Path=/");
    
    response.setBody("Logged in");
    return response;
});
```

#### CORS Headers

```cpp
server.addRoute("/api/data", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"data": "value"})");
    return response;
});
```

#### Redirect Response

```cpp
server.addRoute("/old-page", [](const HTTPRequest& req) {
    HTTPResponse response("301 Moved Permanently");
    response.setHeader("Location", "/new-page");
    return response;
});
```

#### File Download

```cpp
server.addRoute("/download", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/octet-stream");
    response.setHeader("Content-Disposition", "attachment; filename=\"file.txt\"");
    response.setBody("File contents here...");
    return response;
});
```

---

## JSONParser

The `JSONParser` class provides JSON parsing and creation with type-safe accessors.

### Important Note

**All values are stored as strings internally.** Type conversion happens during get/set operations using standard library functions (`std::stoi`, `std::stod`, etc.).

### Constructors

```cpp
// Empty parser
JSONParser();

// Parse from JSON string
explicit JSONParser(const std::string& input);

// Create from existing data
explicit JSONParser(std::map<std::string, std::string> initialData);
```

### File Operations

```cpp
// Load from file (returns pointer, caller owns memory)
JSONParser* json = getJSONFromFile("config.json");

// Save object to file
bool success = saveJSONToFile(json, "output.json");

// Save array to file
bool success = saveArrayJSONToFile(json, "array.json");
```

### Getter Methods

#### Primitive Types

```cpp
std::string getString(const std::string& key);
int getInt(const std::string& key);
short getShort(const std::string& key);
long getLong(const std::string& key);
long long getLongLong(const std::string& key);
bool getBool(const std::string& key);
float getFloat(const std::string& key);
double getDouble(const std::string& key);
long double getLongDouble(const std::string& key);
```

#### Objects and Arrays

```cpp
// Get nested object
JSONParser getObject(const std::string& key);

// Get array of strings
std::vector<std::string> getStringArray(const std::string& key);

// Get array of numbers
std::vector<int> getIntArray(const std::string& key);
std::vector<short> getShortArray(const std::string& key);
std::vector<long> getLongArray(const std::string& key);
std::vector<long long> getLongLongArray(const std::string& key);
std::vector<float> getFloatArray(const std::string& key);
std::vector<double> getDoubleArray(const std::string& key);
std::vector<long double> getLongDoubleArray(const std::string& key);
std::vector<bool> getBoolArray(const std::string& key);

// Get array of objects
std::vector<JSONParser> getArrayOfJSON(const std::string& key);

// Get root-level array
std::vector<JSONParser> getJSONArray();
```

### Setter Methods

#### Primitive Types

```cpp
void setString(const std::string& key, const std::string& value);
void setInt(const std::string& key, int value);
void setShort(const std::string& key, short value);
void setLong(const std::string& key, long value);
void setLongLong(const std::string& key, long long value);
void setBool(const std::string& key, bool value);
void setFloat(const std::string& key, float value);
void setDouble(const std::string& key, double value);
void setLongDouble(const std::string& key, long double value);
```

#### Objects and Arrays

```cpp
void setJSON(const std::string& key, const JSONParser& value);

void setStringArray(const std::string& key, const std::vector<std::string>& value);
void setIntArray(const std::string& key, const std::vector<int>& value);
void setShortArray(const std::string& key, const std::vector<short>& value);
void setLongArray(const std::string& key, const std::vector<long>& value);
void setLongLongArray(const std::string& key, const std::vector<long long>& value);
void setBoolArray(const std::string& key, const std::vector<bool>& value);
void setFloatArray(const std::string& key, const std::vector<float>& value);
void setDoubleArray(const std::string& key, const std::vector<double>& value);
void setLongDoubleArray(const std::string& key, const std::vector<long double>& value);
void setArrayOfJSON(const std::string& key, const std::vector<JSONParser>& value);

// For root-level arrays
void setJSONArray(const std::vector<JSONParser>& value);
void addJSONToArray(const JSONParser& value);
```

### Utility Methods

```cpp
// Get all keys
std::vector<std::string> getKeys();

// Check if key exists
bool hasKey(const std::string& key) const;

// Remove a key
void removeKey(const std::string& key);

// Convert to JSON string
std::string toString() const;

// Convert array to JSON string
std::string arrayToString() const;
```

### Usage Examples

#### Parsing JSON

```cpp
std::string jsonStr = R"({
    "name": "John Doe",
    "age": 30,
    "active": true,
    "balance": 1250.50,
    "tags": ["developer", "gamer"],
    "address": {
        "city": "New York",
        "zip": "10001"
    }
})";

JSONParser json(jsonStr);

std::string name = json.getString("name");      // "John Doe"
int age = json.getInt("age");                   // 30
bool active = json.getBool("active");           // true
double balance = json.getDouble("balance");     // 1250.50

std::vector<std::string> tags = json.getStringArray("tags");
// tags = ["developer", "gamer"]

JSONParser address = json.getObject("address");
std::string city = address.getString("city");   // "New York"
```

#### Creating JSON

```cpp
JSONParser json;

json.setString("message", "Hello, World!");
json.setInt("count", 42);
json.setBool("success", true);
json.setDouble("value", 3.14159);

std::vector<std::string> fruits = {"apple", "banana", "orange"};
json.setStringArray("fruits", fruits);

JSONParser nested;
nested.setString("key", "nested value");
json.setJSON("nested", nested);

std::string output = json.toString();
// {"message":"Hello, World!","count":42,"success":true,...}
```

#### Working with Arrays

```cpp
// Array of objects
std::string arrayJson = R"([
    {"id": 1, "name": "Alice"},
    {"id": 2, "name": "Bob"},
    {"id": 3, "name": "Charlie"}
])";

JSONParser json(arrayJson);
std::vector<JSONParser> users = json.getJSONArray();

for (const auto& user : users) {
    int id = user.getInt("id");
    std::string name = user.getString("name");
    std::cout << id << ": " << name << std::endl;
}
```

#### Building API Responses

```cpp
server.addRoute("/api/users", [](const HTTPRequest& req) {
    JSONParser response;
    response.setBool("success", true);
    
    std::vector<JSONParser> users;
    
    JSONParser user1;
    user1.setInt("id", 1);
    user1.setString("name", "Alice");
    users.push_back(user1);
    
    JSONParser user2;
    user2.setInt("id", 2);
    user2.setString("name", "Bob");
    users.push_back(user2);
    
    response.setArrayOfJSON("users", users);
    
    HTTPResponse httpResp("200 OK");
    httpResp.setHeader("Content-Type", "application/json");
    httpResp.setBody(response.toString());
    return httpResp;
});
```

#### Loading Configuration

```cpp
// config.json
// {
//     "port": 8080,
//     "hostname": "0.0.0.0",
//     "debug": true,
//     "languages": ["en", "de", "fr"]
// }

JSONParser* config = getJSONFromFile("config.json");
if (config) {
    int port = config->getInt("port");
    std::string hostname = config->getString("hostname");
    bool debug = config->getBool("debug");
    std::vector<std::string> languages = config->getStringArray("languages");
    
    server.setPort(port);
    server.setHostname(hostname);
    server.setAvailableLanguages(languages);
    
    delete config;  // Don't forget to free memory!
}
```

---

## Best Practices

### HTTPRequest

- Always check `hasParam()` before `getParam()` to avoid empty strings
- Use `hasHeader()` for optional headers
- Access path segments by index for wildcard routes

### HTTPResponse

- Always set `Content-Type` header
- Use pre-built response functions for common cases
- Use `addHeader()` for cookies and other repeatable headers

### JSONParser

- Check `hasKey()` before accessing optional fields
- Remember to `delete` pointers from `getJSONFromFile()`
- Use correct getter type to avoid conversion errors
- Use `toString()` for objects, `arrayToString()` for root-level arrays

---

## Next Steps

- [Features](FEATURES.md) - Feature overview
- [Translations](TRANSLATIONS.md) - Multi-language support
- [Basic Authentication](BASIC_AUTH.md) - Security features
