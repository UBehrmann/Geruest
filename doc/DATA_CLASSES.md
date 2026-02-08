# Data Classes Reference

## HTTPRequest

Access incoming HTTP request data.

**Core Methods:**
```cpp
std::string getMethod();           // GET, POST, etc.
std::string getPathString();       // Full path (/api/users)
std::string getPath(size_t idx);   // Path segment at index
std::string getClientIP();         // Client IP address
std::string getBody();             // Request body

// Query parameters
bool hasParam(const std::string& name);
std::string getParam(const std::string& name);

// Headers (case-insensitive)
bool hasHeader(const std::string& name);
std::string getHeader(const std::string& name);
```

**Example:**
```cpp
server.addRoute("/search", [](const HTTPRequest& req) {
    if (!req.hasParam("q")) return responseBadRequest();
    std::string query = req.getParam("q");
    std::string page = req.hasParam("page") ? req.getParam("page") : "1";
    
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(R"({"query":")" + query + R"(","page":)" + page + "}");
    return response;
});
```

## HTTPResponse

Build HTTP responses.

**Methods:**
```cpp
HTTPResponse(const std::string& statusCode);  // "200 OK"
void setHeader(const std::string& key, const std::string& value);
void addHeader(const std::string& key, const std::string& value);  // For cookies
void setBody(const std::string& body);  // Auto-sets Content-Length
std::string toString() const;
```

**Pre-built responses:**
```cpp
responseOK(), responseCreated(), responseNoContent(),
responseBadRequest(), responseAuthRequired(), responseForbidden(),
responseNotFound(), responseMethodNotAllowed(), responseConflict(),
responseInternalServerError()
```

**Example:**
```cpp
server.addRoute("/api/data", [](const HTTPRequest& req) {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.addHeader("Set-Cookie", "session=abc; HttpOnly");
    response.setBody(R"({"status":"success"})");
    return response;
});
```

## JSONParser

Parse and create JSON (string-based storage, no external dependencies).

**Constructors:**
```cpp
JSONParser();                                      // Empty
JSONParser(const std::string& jsonStr);            // Parse JSON
JSONParser* getJSONFromFile(const std::string& path);  // Load file
```

**Getters:**
```cpp
std::string getString(const std::string& key);
int getInt(const std::string& key);
bool getBool(const std::string& key);
double getDouble(const std::string& key);
JSONParser getObject(const std::string& key);
std::vector<std::string> getStringArray(const std::string& key);
std::vector<int> getIntArray(const std::string& key);
std::vector<JSONParser> getArrayOfJSON(const std::string& key);
std::vector<JSONParser> getJSONArray();  // Root-level array
```

**Setters:**
```cpp
void setString(const std::string& key, const std::string& value);
void setInt(const std::string& key, int value);
void setBool(const std::string& key, bool value);
void setDouble(const std::string& key, double value);
void setJSON(const std::string& key, const JSONParser& value);
void setStringArray(const std::string& key, const std::vector<std::string>& arr);
void setArrayOfJSON(const std::string& key, const std::vector<JSONParser>& arr);
```

**Utilities:**
```cpp
std::vector<std::string> getKeys();
bool hasKey(const std::string& key) const;
void removeKey(const std::string& key);
std::string toString() const;  // Object to JSON
std::string arrayToString() const;  // Array to JSON
```

**Example:**
```cpp
// Parsing
JSONParser json(R"({"name":"John","age":30,"active":true})");
std::string name = json.getString("name");  // "John"
int age = json.getInt("age");  // 30

// Creating
JSONParser response;
response.setString("message", "Hello");
response.setInt("count", 42);
response.setBool("success", true);
std::string output = response.toString();

// API response
server.addRoute("/api/users", [](const HTTPRequest& req) {
    JSONParser response;
    response.setBool("success", true);
    
    std::vector<JSONParser> users;
    JSONParser user1;
    user1.setInt("id", 1);
    user1.setString("name", "Alice");
    users.push_back(user1);
    
    response.setArrayOfJSON("users", users);
    
    HTTPResponse httpResp("200 OK");
    httpResp.setHeader("Content-Type", "application/json");
    httpResp.setBody(response.toString());
    return httpResp;
});
```

**Important:** All values stored as strings internally - type conversion happens during get/set using `std::stoi`, `std::stod`, etc.