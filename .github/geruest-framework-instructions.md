# Geruest Framework - General Instructions

## Framework Overview

**Geruest** is a C++ web framework designed for building HTTP-based applications with integrated template processing, asset management, and standardized response patterns.

## Core Framework Concepts

### HTTPRequest Class
The `HTTPRequest` class provides comprehensive HTTP request parsing and data access functionality:

#### Core Request Information
```cpp
std::string getMethod() const;           // GET, POST, PUT, DELETE, etc.
std::string getPathString() const;       // Full path as string (e.g., "/api/users")
std::string getPath(size_t index) const; // Path segment by index (0-based)
std::string getClientIP() const;         // Client IP address
std::string getOrigin() const;           // Origin header value
std::string getBody() const;             // Raw request body content
size_t getPathLength() const;            // Number of path segments
```

#### Parameter Access (Unified Interface)
The framework provides a unified parameter access interface that searches across query parameters, JSON body parameters, and cookies:
```cpp
std::string getParam(const std::string& name) const;
bool hasParam(const std::string& name) const;
```

**Parameter Priority Order:**
1. **Query parameters** (URL parameters: `?key=value&key2=value2`)
2. **JSON body parameters** (for `application/json` content)
3. **Cookie values** (HTTP cookies)

**Authentication Parameter Patterns:**
- For API authentication, explicitly pass parameters in query string: `?key=value&user_id=123`
- Framework's `getParam()` will find parameters from cookies, but explicit URL parameters ensure consistency
- Authentication functions should use both `key` and `user_id` parameters for complete validation
- Frontend should extract from cookies and include in API calls as URL parameters

#### Header Access
```cpp
std::string getHeader(const std::string& key) const;  // Case-insensitive header lookup
bool hasHeader(const std::string& key) const;         // Check header existence
```

#### Advanced Features
- **Automatic URL decoding** for query parameters
- **JSON body parsing** when `Content-Type: application/json` is detected
- **Cookie parsing** from Cookie header
- **Path segmentation** for REST-style URLs
- **Multipart form data support** for file uploads
- **Content-Type detection** for automatic body parsing

#### Practical Usage Examples

**RESTful API Route Handling:**
```cpp
HTTPResponse handleUserAPI(const HTTPRequest& req) {
    // Method-based routing
    if (req.getMethod() == "GET") {
        // GET /api/users/{id} - Get user by ID
        if (req.getPathLength() >= 3) {
            std::string userId = req.getPath(2);  // Extract user ID from path
            return getUserById(userId, req);
        }
        // GET /api/users - List all users
        return listUsers(req);
    }
    
    else if (req.getMethod() == "POST") {
        // POST /api/users - Create new user
        return createUser(req);
    }
    
    else if (req.getMethod() == "PUT") {
        // PUT /api/users/{id} - Update user
        if (req.getPathLength() >= 3) {
            std::string userId = req.getPath(2);
            return updateUser(userId, req);
        }
        return responseBadRequest(&req);
    }
    
    else if (req.getMethod() == "DELETE") {
        // DELETE /api/users/{id} - Delete user
        if (req.getPathLength() >= 3) {
            std::string userId = req.getPath(2);
            return deleteUser(userId, req);
        }
        return responseBadRequest(&req);
    }
    
    return responseMethodNotAllowed(&req);
}
```

**Authentication and Parameter Validation:**
```cpp
HTTPResponse secureEndpoint(const HTTPRequest& req) {
    // Authentication check
    if (!req.hasParam("api_key") || !req.hasParam("user_id")) {
        HTTPResponse response = responseAuthRequired(&req);
        response.setBody("{\"error\": \"Authentication required\", \"message\": \"Missing api_key or user_id\"}");
        return response;
    }
    
    // Extract parameters (searches query, JSON body, cookies)
    std::string apiKey = req.getParam("api_key");
    std::string userId = req.getParam("user_id");
    
    // Validate authentication
    if (!validateApiKey(apiKey, userId)) {
        HTTPResponse response = responseForbidden(&req);
        response.setBody("{\"error\": \"Invalid credentials\"}");
        return response;
    }
    
    // Process request based on content type
    if (req.hasHeader("content-type")) {
        std::string contentType = req.getHeader("content-type");
        
        if (contentType.find("application/json") != std::string::npos) {
            // Handle JSON request
            try {
                JSONParser requestData(req.getBody());
                return processJSONRequest(requestData, req);
            } catch (const std::exception& e) {
                HTTPResponse response = responseBadRequest(&req);
                response.setBody("{\"error\": \"Invalid JSON\", \"message\": \"Malformed JSON in request body\"}");
                return response;
            }
        }
        else if (contentType.find("multipart/form-data") != std::string::npos) {
            // Handle file upload
            return processFileUpload(req);
        }
    }
    
    return responseOK(&req);
}
```

**Query Parameter Handling:**
```cpp
HTTPResponse searchEndpoint(const HTTPRequest& req) {
    // Extract pagination parameters with defaults
    int limit = 50;  // default
    int offset = 0;  // default
    
    if (req.hasParam("limit")) {
        try {
            limit = std::stoi(req.getParam("limit"));
            limit = std::max(1, std::min(limit, 1000));  // Clamp between 1-1000
        } catch (...) {
            // Keep default if conversion fails
        }
    }
    
    if (req.hasParam("offset")) {
        try {
            offset = std::max(0, std::stoi(req.getParam("offset")));
        } catch (...) {
            // Keep default if conversion fails
        }
    }
    
    // Extract search parameters
    std::string searchTerm = req.getParam("q");           // Search query
    std::string category = req.getParam("category");      // Filter by category
    std::string sortBy = req.getParam("sort");            // Sort field
    std::string sortOrder = req.getParam("order");        // asc/desc
    
    // Build search query
    SearchParams params;
    params.query = searchTerm;
    params.category = category;
    params.sortBy = sortBy.empty() ? "created_at" : sortBy;
    params.sortOrder = (sortOrder == "desc") ? "DESC" : "ASC";
    params.limit = limit;
    params.offset = offset;
    
    // Execute search and return results
    auto results = performSearch(params);
    return buildSearchResponse(results, req);
}
```

### HTTPResponse Class
The `HTTPResponse` class enables dynamic HTTP response construction with flexible header management and advanced response features:

#### Constructor and Basic Setup
```cpp
HTTPResponse response("200 OK");          // Initialize with status code
HTTPResponse response("404 Not Found");   // Custom status codes supported
response.setBody("Response content");     // Set response body (auto-updates Content-Length)
```

#### Header Management
```cpp
// Single header per key (replaces existing)
response.setHeader("Content-Type", "application/json");
response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
response.setHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");

// Multiple headers with same key (useful for Set-Cookie)
response.addHeader("Set-Cookie", "sessionid=abc123; HttpOnly; Secure; SameSite=Strict");
response.addHeader("Set-Cookie", "theme=dark; Path=/; Max-Age=31536000");
response.addHeader("Set-Cookie", "lang=en; Path=/; Max-Age=31536000");

// Security headers
response.setHeader("X-Content-Type-Options", "nosniff");
response.setHeader("X-Frame-Options", "DENY");
response.setHeader("X-XSS-Protection", "1; mode=block");
```

#### Complete Response Generation
```cpp
std::string fullResponse = response.toString();  // Generates complete HTTP response
```

#### Advanced Response Patterns

**JSON API Response with Custom Headers:**
```cpp
HTTPResponse buildSecureAPIResponse(const HTTPRequest& req, const JSONParser& data) {
    HTTPResponse response = responseOK(&req);        // Framework helper with CORS
    
    // Content settings
    response.setHeader("Content-Type", "application/json; charset=utf-8");
    response.setBody(data.toString());
    
    // Security headers
    response.setHeader("X-Content-Type-Options", "nosniff");
    response.setHeader("X-Frame-Options", "DENY");
    response.setHeader("Referrer-Policy", "strict-origin-when-cross-origin");
    
    // Cache control for API responses
    response.setHeader("Cache-Control", "no-store, must-revalidate");
    response.setHeader("Pragma", "no-cache");
    
    return response;
}
```

**File Download Response:**
```cpp
HTTPResponse serveFile(const HTTPRequest& req, const std::string& filePath, const std::string& fileName) {
    HTTPResponse response = responseOK(&req);
    
    // Read file content (implement based on your file handling)
    std::string fileContent = readFileContent(filePath);
    
    if (fileContent.empty()) {
        return responseNotFound(&req);
    }
    
    // Set appropriate content type based on file extension
    std::string contentType = getContentTypeFromExtension(fileName);
    response.setHeader("Content-Type", contentType);
    
    // Force download
    response.setHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
    
    // Set content length
    response.setHeader("Content-Length", std::to_string(fileContent.length()));
    
    response.setBody(fileContent);
    return response;
}
```

**Redirect Response:**
```cpp
HTTPResponse redirectTo(const std::string& location, bool permanent = false) {
    std::string statusCode = permanent ? "301 Moved Permanently" : "302 Found";
    HTTPResponse response(statusCode);
    
    response.setHeader("Location", location);
    response.setHeader("Cache-Control", "no-store");
    
    // Optional: Set a body with redirect message
    response.setBody("<!DOCTYPE html><html><body>Redirecting to <a href=\"" + 
                    location + "\">" + location + "</a></body></html>");
    response.setHeader("Content-Type", "text/html; charset=utf-8");
    
    return response;
}
```

**Stream Response for Large Data:**
```cpp
HTTPResponse streamResponse(const HTTPRequest& req, const std::string& contentType) {
    HTTPResponse response = responseOK(&req);
    
    response.setHeader("Content-Type", contentType);
    response.setHeader("Transfer-Encoding", "chunked");
    response.setHeader("Connection", "keep-alive");
    
    // For streaming, you would typically not set the body immediately
    // Instead, use chunked encoding to send data progressively
    return response;
}
```

**Custom Error Response with Details:**
```cpp
HTTPResponse createDetailedErrorResponse(const HTTPRequest& req, int statusCode, 
                                       const std::string& error, const std::string& message) {
    HTTPResponse response;
    
    // Map status codes to appropriate responses
    switch (statusCode) {
        case 400: response = responseBadRequest(&req); break;
        case 401: response = responseAuthRequired(&req); break;
        case 403: response = responseForbidden(&req); break;
        case 404: response = responseNotFound(&req); break;
        case 409: response = responseConflict(&req); break;
        case 500: response = responseInternalServerError(&req); break;
        default:  response = responseInternalServerError(&req); break;
    }
    
    // Create detailed error JSON
    JSONParser errorJson;
    errorJson.setString("error", error);
    errorJson.setString("message", message);
    errorJson.setInt("status", statusCode);
    errorJson.setString("timestamp", getCurrentTimestamp());
    
    // Add request context for debugging
    JSONParser requestInfo;
    requestInfo.setString("method", req.getMethod());
    requestInfo.setString("path", req.getPathString());
    requestInfo.setString("ip", req.getClientIP());
    errorJson.setJSON("request", requestInfo);
    
    response.setBody(errorJson.toString());
    return response;
}
```

#### Response Construction Best Practices

**Standard API Response Builder:**
```cpp
class ResponseBuilder {
public:
    static HTTPResponse success(const HTTPRequest& req, const JSONParser& data) {
        HTTPResponse response = responseOK(&req);
        
        JSONParser wrapper;
        wrapper.setBool("success", true);
        wrapper.setJSON("data", data);
        wrapper.setString("timestamp", getCurrentTimestamp());
        
        response.setHeader("Content-Type", "application/json; charset=utf-8");
        response.setBody(wrapper.toString());
        return response;
    }
    
    static HTTPResponse error(const HTTPRequest& req, int statusCode, 
                            const std::string& error, const std::string& message = "") {
        HTTPResponse response;
        
        // Use framework helpers based on status code
        switch (statusCode) {
            case 400: response = responseBadRequest(&req); break;
            case 401: response = responseAuthRequired(&req); break;
            case 403: response = responseForbidden(&req); break;
            case 404: response = responseNotFound(&req); break;
            case 500: response = responseInternalServerError(&req); break;
            default:  response = responseInternalServerError(&req); break;
        }
        
        JSONParser errorJson;
        errorJson.setBool("success", false);
        errorJson.setString("error", error);
        if (!message.empty()) {
            errorJson.setString("message", message);
        }
        errorJson.setString("timestamp", getCurrentTimestamp());
        
        response.setBody(errorJson.toString());
        return response;
    }
    
    static HTTPResponse paginated(const HTTPRequest& req, const JSONParser& data, 
                                int total, int offset, int limit) {
        HTTPResponse response = responseOK(&req);
        
        JSONParser wrapper;
        wrapper.setBool("success", true);
        wrapper.setJSON("data", data);
        
        // Pagination metadata
        JSONParser pagination;
        pagination.setInt("total", total);
        pagination.setInt("offset", offset);
        pagination.setInt("limit", limit);
        pagination.setBool("hasMore", (offset + limit) < total);
        wrapper.setJSON("pagination", pagination);
        
        response.setHeader("Content-Type", "application/json; charset=utf-8");
        response.setBody(wrapper.toString());
        return response;
    }
};
```

#### Cookie Management
```cpp
HTTPResponse setAuthenticationCookies(const HTTPRequest& req, const std::string& sessionId, 
                                    const std::string& userId) {
    HTTPResponse response = responseOK(&req);
    
    // Session cookie (expires when browser closes)
    response.addHeader("Set-Cookie", "session_id=" + sessionId + 
                      "; HttpOnly; Secure; SameSite=Strict; Path=/");
    
    // User ID cookie (persistent for 30 days)
    response.addHeader("Set-Cookie", "user_id=" + userId + 
                      "; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=2592000");
    
    // CSRF token
    std::string csrfToken = generateCSRFToken();
    response.addHeader("Set-Cookie", "csrf_token=" + csrfToken + 
                      "; HttpOnly; Secure; SameSite=Strict; Path=/");
    
    JSONParser responseData;
    responseData.setBool("authenticated", true);
    responseData.setString("message", "Login successful");
    
    response.setBody(responseData.toString());
    return response;
}

HTTPResponse clearAuthenticationCookies(const HTTPRequest& req) {
    HTTPResponse response = responseOK(&req);
    
    // Clear all authentication cookies
    response.addHeader("Set-Cookie", "session_id=; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=0");
    response.addHeader("Set-Cookie", "user_id=; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=0");
    response.addHeader("Set-Cookie", "csrf_token=; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=0");
    
    JSONParser responseData;
    responseData.setBool("success", true);
    responseData.setString("message", "Logged out successfully");
    
    response.setBody(responseData.toString());
    return response;
}
```

### JSONParser Class
The `JSONParser` class provides comprehensive JSON parsing and generation capabilities for the Geruest framework:

#### Framework Integration Status
**✅ JSONParser is now fully integrated into Geruest framework**
- Available via `#include <Geruest.hpp>` and `using namespace geruest;`
- Successfully replaces manual JSON string construction
- Used in production code for database JSON serialization
- Provides type-safe JSON operations with proper escaping

#### How to Use JSONParser in Your Project
```cpp
#include <Geruest.hpp>
using namespace geruest;

// Create JSONParser instance
JSONParser parser("{\"key\": \"value\"}");
std::string value = parser.getString("key");

// Create JSON objects for API responses
JSONParser response;
response.setString("status", "success");
response.setInt("user_id", 12345);
return response.toString(); // Returns: {"status":"success","user_id":12345}
```

#### Construction and Initialization
```cpp
// Parse JSON from string
JSONParser json("{\"name\": \"John\", \"age\": 30}");

// Create empty JSON object
JSONParser json;

// Create from std::map
std::map<std::string, std::any> data = {{"key", std::string("value")}};
JSONParser json(data);
```

#### Data Access Methods
**String Values:**
```cpp
std::string name = json.getString("name");
json.setString("name", "John Doe");
```

**Numeric Values:**
```cpp
int age = json.getInt("age");
long id = json.getLong("id");
float price = json.getFloat("price");
double precision = json.getDouble("precision");
bool active = json.getBool("active");

json.setInt("age", 25);
json.setLong("id", 12345L);
json.setFloat("price", 19.99f);
json.setDouble("precision", 3.14159);
json.setBool("active", true);
```

**Nested Objects:**
```cpp
JSONParser userObj = json.getObject("user");
JSONParser newUser;
newUser.setString("name", "Jane");
json.setJSON("user", newUser);
```

#### Array Handling
**String Arrays:**
```cpp
std::vector<std::string> tags = json.getStringArray("tags");
std::vector<std::string> newTags = {"web", "api", "json"};
json.setStringArray("tags", newTags);
```

**Numeric Arrays:**
```cpp
std::vector<int> numbers = json.getIntArray("numbers");
std::vector<float> prices = json.getFloatArray("prices");
std::vector<bool> flags = json.getBoolArray("flags");

json.setIntArray("numbers", {1, 2, 3, 4, 5});
json.setFloatArray("prices", {9.99f, 19.99f, 29.99f});
json.setBoolArray("flags", {true, false, true});
```

**JSON Object Arrays:**
```cpp
std::vector<JSONParser> users = json.getArrayOfJSON("users");
std::vector<JSONParser> newUsers;
// ... populate newUsers ...
json.setArrayOfJSON("users", newUsers);
```

#### Array Handling - Understanding arrayToString() vs toString()

**Critical Concept: Two Types of JSON Structures**

The JSONParser class supports two fundamental JSON structures:
1. **JSON Objects** - Key-value pairs: `{"key": "value", "number": 42}`
2. **JSON Arrays** - Lists of items: `[{"id": 1}, {"id": 2}, {"id": 3}]`

**When to Use arrayToString() vs toString():**

```cpp
// === JSON OBJECTS (use toString()) ===
// Structure: {"key1": value1, "key2": value2}
JSONParser objectJson;
objectJson.setString("name", "John");
objectJson.setInt("age", 30);
objectJson.setBool("active", true);

std::string result = objectJson.toString();
// Result: {"name":"John","age":30,"active":true}
```

```cpp
// === JSON ARRAYS (use arrayToString()) ===
// Structure: [item1, item2, item3]

// Method 1: Using setJSONArray()
JSONParser item1, item2;
item1.setString("name", "John");
item1.setInt("id", 1);
item2.setString("name", "Jane");
item2.setInt("id", 2);

JSONParser arrayJson;
std::vector<JSONParser> items = {item1, item2};
arrayJson.setJSONArray(items);

std::string result = arrayJson.arrayToString();
// Result: [{"name":"John","id":1},{"name":"Jane","id":2}]
```

```cpp
// Method 2: Using addJSONToArray() (incremental building)
JSONParser arrayJson;
arrayJson.addJSONToArray(item1);
arrayJson.addJSONToArray(item2);

std::string result = arrayJson.arrayToString();
// Result: [{"name":"John","id":1},{"name":"Jane","id":2}]
```

**Common Pattern: Array Within Object (API Responses)**
```cpp
// Most APIs return objects containing arrays, not raw arrays
JSONParser responseWrapper;
responseWrapper.setBool("success", true);
responseWrapper.setInt("total", 2);
responseWrapper.setArrayOfJSON("users", {item1, item2});  // Array as object property

std::string result = responseWrapper.toString();  // Use toString() for wrapper
// Result: {"success":true,"total":2,"users":[{"name":"John","id":1},{"name":"Jane","id":2}]}
```

**Parsing JSON Arrays from Strings:**
```cpp
// Parse a JSON array string
std::string jsonArrayString = R"([{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}])";
JSONParser parsedArray(jsonArrayString);

// Access the parsed array
std::vector<JSONParser> items = parsedArray.getJSONArray();
std::cout << "Found " << items.size() << " items" << std::endl;

// Convert back to string
std::string result = parsedArray.arrayToString();
// Result: [{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}]
```

**❌ Common Mistakes:**
```cpp
// WRONG: Using toString() on array structure
JSONParser arrayJson;
arrayJson.setJSONArray({item1, item2});
std::string wrong = arrayJson.toString();  // This won't work as expected!

// WRONG: Using arrayToString() on object structure  
JSONParser objectJson;
objectJson.setString("key", "value");
std::string wrong = objectJson.arrayToString();  // Returns empty array []
```

**✅ Correct Usage Summary:**
- **`toString()`** → Use for JSON objects with key-value pairs
- **`arrayToString()`** → Use for JSON arrays (list of items)
- **Most API responses** → Use `toString()` because they're objects containing arrays
- **Direct array responses** → Use `arrayToString()` (less common in APIs)

#### Utility Methods
```cpp
// Get all keys in JSON object
std::vector<std::string> keys = json.getKeys();

// Check if a key exists
bool exists = json.hasKey("optional_field");

// Remove a key
json.removeKey("unwanted_field");

// Convert to string representation
std::string jsonString = json.toString();      // For JSON objects
std::string arrayString = json.arrayToString(); // For JSON arrays
```

#### Complete Real-World Examples

**Example 1: User Management API - List Users**
```cpp
HTTPResponse getUserListAPI(const HTTPRequest& req) {
    try {
        // Get users from database
        auto users = database.getUsers();
        
        // Build array of user objects
        std::vector<JSONParser> userArray;
        for (const auto& user : users) {
            JSONParser userObj;
            userObj.setInt("id", user.id);
            userObj.setString("name", user.name);
            userObj.setString("email", user.email);
            userObj.setBool("active", user.active);
            userArray.push_back(userObj);
        }
        
        // OPTION 1: Wrapped response (RECOMMENDED for APIs)
        JSONParser response;
        response.setBool("success", true);
        response.setInt("total", userArray.size());
        response.setArrayOfJSON("users", userArray);
        
        HTTPResponse httpResponse = responseOK(&req);
        httpResponse.setBody(response.toString());  // Use toString() for wrapper object
        // Result: {"success":true,"total":3,"users":[{"id":1,"name":"John",...}]}
        return httpResponse;
        
    } catch (const std::exception& e) {
        return handleError(req, e);
    }
}
```

**Example 2: Direct Array Response (Less Common)**
```cpp
HTTPResponse getUserArrayAPI(const HTTPRequest& req) {
    try {
        auto users = database.getUsers();
        
        // Build JSON array directly
        JSONParser userArray;
        for (const auto& user : users) {
            JSONParser userObj;
            userObj.setInt("id", user.id);
            userObj.setString("name", user.name);
            userObj.setBool("active", user.active);
            userArray.addJSONToArray(userObj);  // Add to array incrementally
        }
        
        HTTPResponse httpResponse = responseOK(&req);
        httpResponse.setBody(userArray.arrayToString());  // Use arrayToString() for direct arrays
        // Result: [{"id":1,"name":"John","active":true},{"id":2,"name":"Jane","active":false}]
        return httpResponse;
        
    } catch (const std::exception& e) {
        return handleError(req, e);
    }
}
```

**Example 3: Parsing Client JSON Array**
```cpp
HTTPResponse processBatchUsersAPI(const HTTPRequest& req) {
    try {
        // Parse JSON array from request body: [{"name":"John","age":30},{"name":"Jane","age":25}]
        JSONParser requestArray(req.getBody());
        
        // Get array items
        std::vector<JSONParser> userItems = requestArray.getJSONArray();
        
        std::vector<int> createdUserIds;
        for (const auto& userItem : userItems) {
            // Extract user data
            std::string name = userItem.getString("name");
            int age = userItem.getInt("age");
            bool active = userItem.getBool("active");  // defaults to false if not present
            
            // Validate and create user
            if (name.empty() || age <= 0) {
                JSONParser error;
                error.setString("error", "ValidationError");
                error.setString("message", "Invalid user data: name and age required");
                
                HTTPResponse response = responseBadRequest(&req);
                response.setBody(error.toString());
                return response;
            }
            
            // Create user in database
            int userId = database.createUser(name, age, active);
            createdUserIds.push_back(userId);
        }
        
        // Build success response
        JSONParser response;
        response.setBool("success", true);
        response.setString("message", "Users created successfully");
        response.setInt("created_count", createdUserIds.size());
        response.setIntArray("created_ids", createdUserIds);
        
        HTTPResponse httpResponse = responseCreated(&req);
        httpResponse.setBody(response.toString());  // Object response uses toString()
        return httpResponse;
        
    } catch (const std::invalid_argument& e) {
        // JSON parsing failed
        JSONParser error;
        error.setString("error", "InvalidJSON");
        error.setString("message", "Request body must be a valid JSON array");
        
        HTTPResponse response = responseBadRequest(&req);
        response.setBody(error.toString());
        return response;
    }
}
```

**Example 4: Configuration File with Arrays**
```cpp
class ConfigManager {
public:
    bool loadConfig(const std::string& configPath) {
        JSONParser* config = getJSONFromFile(configPath);
        if (!config) return false;
        
        // Parse server settings
        if (config->hasKey("server")) {
            JSONParser serverConfig = config->getObject("server");
            serverPort = serverConfig.getInt("port");
            serverHost = serverConfig.getString("host");
            enableSSL = serverConfig.getBool("enable_ssl");
        }
        
        // Parse allowed origins array
        if (config->hasKey("allowed_origins")) {
            allowedOrigins = config->getStringArray("allowed_origins");
        }
        
        // Parse database pool settings
        if (config->hasKey("database_pools")) {
            std::vector<JSONParser> poolConfigs = config->getArrayOfJSON("database_pools");
            
            for (const auto& poolConfig : poolConfigs) {
                DatabasePool pool;
                pool.name = poolConfig.getString("name");
                pool.host = poolConfig.getString("host");
                pool.port = poolConfig.getInt("port");
                pool.maxConnections = poolConfig.getInt("max_connections");
                databasePools.push_back(pool);
            }
        }
        
        delete config;
        return true;
    }
    
    bool saveConfig(const std::string& configPath) {
        JSONParser config;
        
        // Server settings
        JSONParser serverConfig;
        serverConfig.setInt("port", serverPort);
        serverConfig.setString("host", serverHost);
        serverConfig.setBool("enable_ssl", enableSSL);
        config.setJSON("server", serverConfig);
        
        // Allowed origins
        config.setStringArray("allowed_origins", allowedOrigins);
        
        // Database pools
        std::vector<JSONParser> poolConfigs;
        for (const auto& pool : databasePools) {
            JSONParser poolConfig;
            poolConfig.setString("name", pool.name);
            poolConfig.setString("host", pool.host);
            poolConfig.setInt("port", pool.port);
            poolConfig.setInt("max_connections", pool.maxConnections);
            poolConfigs.push_back(poolConfig);
        }
        config.setArrayOfJSON("database_pools", poolConfigs);
        
        return saveJSONToFile(config, configPath);  // Uses toString() internally
    }
};
```

**Example 5: Debugging and Validation**
```cpp
void debugJSONStructure(const JSONParser& json) {
    std::cout << "JSON Keys: ";
    auto keys = json.getKeys();
    for (const auto& key : keys) {
        std::cout << key << " ";
    }
    std::cout << std::endl;
    
    // Check if it's an array or object
    if (keys.empty()) {
        auto arrayData = json.getJSONArray();
        if (!arrayData.empty()) {
            std::cout << "This is a JSON array with " << arrayData.size() << " items" << std::endl;
            std::cout << "Array content: " << json.arrayToString() << std::endl;
        } else {
            std::cout << "This is an empty JSON structure" << std::endl;
        }
    } else {
        std::cout << "This is a JSON object" << std::endl;
        std::cout << "Object content: " << json.toString() << std::endl;
    }
}

// Validation helper
bool validateUserJSON(const JSONParser& user, std::string& errorMessage) {
    if (!user.hasKey("name") || user.getString("name").empty()) {
        errorMessage = "Missing or empty 'name' field";
        return false;
    }
    
    if (!user.hasKey("email") || user.getString("email").find("@") == std::string::npos) {
        errorMessage = "Missing or invalid 'email' field";
        return false;
    }
    
    int age = user.getInt("age");
    if (age <= 0 || age > 150) {
        errorMessage = "Invalid 'age' field (must be 1-150)";
        return false;
    }
    
    return true;
}
```

**Key Takeaways:**
1. **toString()** is for JSON objects (most API responses)
2. **arrayToString()** is for JSON arrays (direct array responses or when debugging arrays)
3. **Most APIs use wrapped responses** - objects containing arrays, not raw arrays
4. **Use hasKey()** to check for optional fields before accessing them
5. **Parse arrays with getJSONArray()**, parse objects normally
6. **Build arrays incrementally** with addJSONToArray() or all at once with setJSONArray()

#### File I/O Operations with Correct Method Usage
```cpp
// Load JSON from file (automatically detects if it's an object or array)
JSONParser* json = getJSONFromFile("/path/to/file.json");
if (json != nullptr) {
    // Check what type of JSON it is
    auto keys = json->getKeys();
    if (keys.empty()) {
        // It's likely a JSON array
        auto arrayData = json->getJSONArray();
        if (!arrayData.empty()) {
            std::cout << "Loaded JSON array with " << arrayData.size() << " items" << std::endl;
            std::string content = json->arrayToString();
        }
    } else {
        // It's a JSON object
        std::cout << "Loaded JSON object with keys: ";
        for (const auto& key : keys) {
            std::cout << key << " ";
        }
        std::string content = json->toString();
    }
    
    delete json;  // Remember to clean up
}

// Save JSON object to file
JSONParser configJson;
configJson.setString("app_name", "MyApp");
configJson.setInt("version", 1);
configJson.setBool("debug_mode", false);

bool success = saveJSONToFile(configJson, "/path/to/config.json");
// File content: {"app_name":"MyApp","version":1,"debug_mode":false}

// Save JSON array to file using saveArrayJSONToFile
JSONParser item1, item2;
item1.setString("name", "John");
item1.setInt("id", 1);
item2.setString("name", "Jane");
item2.setInt("id", 2);

JSONParser arrayJson;
arrayJson.setJSONArray({item1, item2});

bool success = saveArrayJSONToFile(arrayJson, "/path/to/users.json");
// File content: [{"name":"John","id":1},{"name":"Jane","id":2}]

// Alternative: Save array using regular saveJSONToFile (manual string conversion)
std::ofstream file("/path/to/users_alt.json");
if (file.is_open()) {
    file << arrayJson.arrayToString();  // Use arrayToString() for arrays
    file.close();
}

// Load and process array file
JSONParser* userArray = getJSONFromFile("/path/to/users.json");
if (userArray != nullptr) {
    std::vector<JSONParser> users = userArray->getJSONArray();
    
    for (const auto& user : users) {
        std::string name = user.getString("name");
        int id = user.getInt("id");
        std::cout << "User: " << name << " (ID: " << id << ")" << std::endl;
    }
    
    delete userArray;
}
```

#### Troubleshooting arrayToString() Issues

**Problem 1: arrayToString() returns empty array `[]`**
```cpp
// CAUSE: Using arrayToString() on a regular JSON object
JSONParser json;
json.setString("name", "John");
json.setInt("age", 30);
std::string result = json.arrayToString();  // Returns: []

// SOLUTION: Use toString() for objects
std::string result = json.toString();  // Returns: {"name":"John","age":30}
```

**Problem 2: toString() doesn't show array content properly**
```cpp
// CAUSE: Using toString() on a JSON array structure  
JSONParser item;
item.setString("name", "John");

JSONParser arrayJson;
arrayJson.addJSONToArray(item);
std::string result = arrayJson.toString();  // Returns: {} (empty object)

// SOLUTION: Use arrayToString() for arrays
std::string result = arrayJson.arrayToString();  // Returns: [{"name":"John"}]
```

**Problem 3: Array within object not displaying**
```cpp
// CAUSE: Forgetting that arrays within objects are handled automatically
JSONParser response;
response.setBool("success", true);

std::vector<JSONParser> items = {item1, item2};
response.setArrayOfJSON("users", items);

// WRONG: Trying to use arrayToString() on the wrapper
std::string wrong = response.arrayToString();  // Returns: []

// CORRECT: Use toString() - it handles nested arrays automatically
std::string correct = response.toString();  // Returns: {"success":true,"users":[...]}
```

**Problem 4: Parsing JSON array returns empty**
```cpp
// CAUSE: JSON array string not properly formatted or parsed
std::string badJson = "{\"users\": [{\"name\":\"John\"}]}";  // This is an object, not array
JSONParser parsed(badJson);
auto array = parsed.getJSONArray();  // Returns empty vector

// SOLUTION: Parse as object and extract array
JSONParser parsed(badJson);
std::vector<JSONParser> users = parsed.getArrayOfJSON("users");  // Correct way

// OR use actual JSON array string:
std::string goodJson = "[{\"name\":\"John\"},{\"name\":\"Jane\"}]";
JSONParser parsedArray(goodJson);
auto array = parsedArray.getJSONArray();  // Returns 2 items
```

**Problem 5: Boolean values showing as numbers**
```cpp
// This was fixed in our implementation - booleans now properly serialize as true/false

JSONParser json;
json.setBool("active", true);
json.setBool("deleted", false);
std::string result = json.toString();
// Returns: {"active":true,"deleted":false}  (not {"active":1,"deleted":0})
```

**Quick Debug Helper:**
```cpp
void debugJSONType(const JSONParser& json, const std::string& label) {
    std::cout << "=== " << label << " ===" << std::endl;
    
    auto keys = json.getKeys();
    if (!keys.empty()) {
        std::cout << "Type: JSON Object" << std::endl;
        std::cout << "Keys: " << keys.size() << std::endl;
        std::cout << "Content (toString): " << json.toString() << std::endl;
        std::cout << "Content (arrayToString): " << json.arrayToString() << std::endl;
    } else {
        auto arrayData = json.getJSONArray();
        if (!arrayData.empty()) {
            std::cout << "Type: JSON Array" << std::endl;
            std::cout << "Items: " << arrayData.size() << std::endl;
            std::cout << "Content (arrayToString): " << json.arrayToString() << std::endl;
            std::cout << "Content (toString): " << json.toString() << std::endl;
        } else {
            std::cout << "Type: Empty JSON" << std::endl;
        }
    }
    std::cout << "===================" << std::endl;
}
```

#### Common Usage Patterns

**Complete API Request/Response Handling:**
```cpp
HTTPResponse handleUserCreationAPI(const HTTPRequest& req) {
    // Validate content type
    if (!req.hasHeader("content-type") || 
        req.getHeader("content-type").find("application/json") == std::string::npos) {
        
        JSONParser error;
        error.setString("error", "InvalidContentType");
        error.setString("message", "Content-Type must be application/json");
        
        HTTPResponse response = responseBadRequest(&req);
        response.setBody(error.toString());
        return response;
    }
    
    try {
        JSONParser requestData(req.getBody());
        
        // Extract and validate required fields
        std::string name = requestData.getString("name");
        std::string email = requestData.getString("email");
        int age = requestData.getInt("age");
        
        // Field validation
        std::vector<std::string> errors;
        
        if (name.empty() || name.length() < 2) {
            errors.push_back("Name must be at least 2 characters long");
        }
        
        if (email.empty() || email.find("@") == std::string::npos) {
            errors.push_back("Valid email address is required");
        }
        
        if (age <= 0 || age > 150) {
            errors.push_back("Age must be between 1 and 150");
        }
        
        // Return validation errors
        if (!errors.empty()) {
            JSONParser errorResponse;
            errorResponse.setString("error", "ValidationFailed");
            errorResponse.setString("message", "One or more fields are invalid");
            errorResponse.setStringArray("details", errors);
            
            HTTPResponse response = responseBadRequest(&req);
            response.setBody(errorResponse.toString());
            return response;
        }
        
        // Extract optional fields with defaults
        bool isActive = requestData.getBool("active");  // defaults to false if not present
        std::vector<std::string> tags = requestData.getStringArray("tags");  // empty if not present
        
        // Create user object (your implementation)
        User newUser;
        newUser.setName(name);
        newUser.setEmail(email);
        newUser.setAge(age);
        newUser.setActive(isActive);
        newUser.setTags(tags);
        
        // Save to database (your implementation)
        int userId = saveUser(newUser);
        
        // Create success response
        JSONParser responseData;
        responseData.setString("status", "success");
        responseData.setString("message", "User created successfully");
        responseData.setInt("user_id", userId);
        
        // Include created user data
        JSONParser userData;
        userData.setString("name", name);
        userData.setString("email", email);
        userData.setInt("age", age);
        userData.setBool("active", isActive);
        userData.setStringArray("tags", tags);
        responseData.setJSON("user", userData);
        
        HTTPResponse response = responseCreated(&req);  // 201 Created
        response.setBody(responseData.toString());
        return response;
        
    } catch (const std::invalid_argument& e) {
        // JSON parsing error
        JSONParser error;
        error.setString("error", "InvalidJSON");
        error.setString("message", "Malformed JSON in request body");
        error.setString("details", e.what());
        
        HTTPResponse response = responseBadRequest(&req);
        response.setBody(error.toString());
        return response;
        
    } catch (const std::exception& e) {
        // Other errors
        JSONParser error;
        error.setString("error", "InternalServerError");
        error.setString("message", "An unexpected error occurred");
        
        HTTPResponse response = responseInternalServerError(&req);
        response.setBody(error.toString());
        return response;
    }
}
```

**Advanced Configuration Management:**
```cpp
class ConfigurationManager {
private:
    JSONParser* config;
    std::string configPath;
    
public:
    ConfigurationManager(const std::string& path) : configPath(path), config(nullptr) {
        reload();
    }
    
    ~ConfigurationManager() {
        if (config) {
            delete config;
        }
    }
    
    bool reload() {
        if (config) {
            delete config;
            config = nullptr;
        }
        
        config = getJSONFromFile(configPath);
        return config != nullptr;
    }
    
    // Database configuration
    struct DatabaseConfig {
        std::string host;
        int port;
        std::string database;
        std::string username;
        std::string password;
        bool useSSL;
        int maxConnections;
    };
    
    DatabaseConfig getDatabaseConfig() {
        DatabaseConfig dbConfig;
        
        if (config && config->hasKey("database")) {
            JSONParser dbSection = config->getObject("database");
            
            dbConfig.host = dbSection.getString("host");
            dbConfig.port = dbSection.getInt("port");
            dbConfig.database = dbSection.getString("database");
            dbConfig.username = dbSection.getString("username");
            dbConfig.password = dbSection.getString("password");
            dbConfig.useSSL = dbSection.getBool("use_ssl");
            dbConfig.maxConnections = dbSection.getInt("max_connections");
        }
        
        return dbConfig;
    }
    
    // Server configuration
    struct ServerConfig {
        int port;
        std::string host;
        bool enableHTTPS;
        std::string certFile;
        std::string keyFile;
        std::vector<std::string> allowedOrigins;
    };
    
    ServerConfig getServerConfig() {
        ServerConfig serverConfig;
        serverConfig.port = 8080;  // default
        serverConfig.host = "localhost";  // default
        
        if (config && config->hasKey("server")) {
            JSONParser serverSection = config->getObject("server");
            
            serverConfig.port = serverSection.getInt("port");
            serverConfig.host = serverSection.getString("host");
            serverConfig.enableHTTPS = serverSection.getBool("enable_https");
            serverConfig.certFile = serverSection.getString("cert_file");
            serverConfig.keyFile = serverSection.getString("key_file");
            serverConfig.allowedOrigins = serverSection.getStringArray("allowed_origins");
        }
        
        return serverConfig;
    }
    
    // Application settings
    std::string getAppSetting(const std::string& key, const std::string& defaultValue = "") {
        if (config && config->hasKey("application") && config->getObject("application").hasKey(key)) {
            return config->getObject("application").getString(key);
        }
        return defaultValue;
    }
    
    int getAppSettingInt(const std::string& key, int defaultValue = 0) {
        if (config && config->hasKey("application") && config->getObject("application").hasKey(key)) {
            return config->getObject("application").getInt(key);
        }
        return defaultValue;
    }
};
```

**Complex Data Structure Serialization:**
```cpp
// User profile with nested objects and arrays
JSONParser serializeUserProfile(const User& user) {
    JSONParser profile;
    
    // Basic user information
    profile.setString("id", user.getId());
    profile.setString("username", user.getUsername());
    profile.setString("email", user.getEmail());
    profile.setString("created_at", user.getCreatedAt());
    profile.setBool("active", user.isActive());
    
    // User preferences (nested object)
    JSONParser preferences;
    preferences.setString("theme", user.getPreferences().theme);
    preferences.setString("language", user.getPreferences().language);
    preferences.setBool("notifications_enabled", user.getPreferences().notificationsEnabled);
    preferences.setInt("items_per_page", user.getPreferences().itemsPerPage);
    profile.setJSON("preferences", preferences);
    
    // User roles (array of strings)
    std::vector<std::string> roles = user.getRoles();
    profile.setStringArray("roles", roles);
    
    // User permissions (array of objects)
    std::vector<JSONParser> permissions;
    for (const auto& permission : user.getPermissions()) {
        JSONParser permJson;
        permJson.setString("resource", permission.resource);
        permJson.setString("action", permission.action);
        permJson.setBool("granted", permission.granted);
        permissions.push_back(permJson);
    }
    profile.setArrayOfJSON("permissions", permissions);
    
    // Recent activity (array of objects with mixed data types)
    std::vector<JSONParser> activities;
    for (const auto& activity : user.getRecentActivities()) {
        JSONParser activityJson;
        activityJson.setString("id", activity.id);
        activityJson.setString("type", activity.type);
        activityJson.setString("timestamp", activity.timestamp);
        
        // Activity-specific data (nested object)
        JSONParser activityData;
        for (const auto& pair : activity.data) {
            activityData.setString(pair.first, pair.second);
        }
        activityJson.setJSON("data", activityData);
        
        activities.push_back(activityJson);
    }
    profile.setArrayOfJSON("recent_activities", activities);
    
    return profile;
}

// Deserialize user profile from JSON
User deserializeUserProfile(const JSONParser& profileJson) {
    User user;
    
    // Basic information
    user.setId(profileJson.getString("id"));
    user.setUsername(profileJson.getString("username"));
    user.setEmail(profileJson.getString("email"));
    user.setCreatedAt(profileJson.getString("created_at"));
    user.setActive(profileJson.getBool("active"));
    
    // Preferences
    if (profileJson.hasKey("preferences")) {
        JSONParser prefs = profileJson.getObject("preferences");
        UserPreferences preferences;
        preferences.theme = prefs.getString("theme");
        preferences.language = prefs.getString("language");
        preferences.notificationsEnabled = prefs.getBool("notifications_enabled");
        preferences.itemsPerPage = prefs.getInt("items_per_page");
        user.setPreferences(preferences);
    }
    
    // Roles
    if (profileJson.hasKey("roles")) {
        std::vector<std::string> roles = profileJson.getStringArray("roles");
        user.setRoles(roles);
    }
    
    // Permissions
    if (profileJson.hasKey("permissions")) {
        std::vector<JSONParser> permissionsJson = profileJson.getArrayOfJSON("permissions");
        std::vector<UserPermission> permissions;
        
        for (const auto& permJson : permissionsJson) {
            UserPermission perm;
            perm.resource = permJson.getString("resource");
            perm.action = permJson.getString("action");
            perm.granted = permJson.getBool("granted");
            permissions.push_back(perm);
        }
        user.setPermissions(permissions);
    }
    
    return user;
}
```

**Database Result Set Serialization:**
```cpp
JSONParser serializeDatabaseResults(const std::vector<std::map<std::string, std::string>>& results) {
    JSONParser responseJson;
    
    // Array to hold all records
    std::vector<JSONParser> records;
    
    for (const auto& row : results) {
        JSONParser record;
        
        for (const auto& column : row) {
            const std::string& columnName = column.first;
            const std::string& value = column.second;
            
            // Try to determine data type and set appropriately
            if (value.empty()) {
                record.setString(columnName, "");
            }
            else if (isInteger(value)) {
                record.setInt(columnName, std::stoi(value));
            }
            else if (isFloat(value)) {
                record.setFloat(columnName, std::stof(value));
            }
            else if (isBoolean(value)) {
                record.setBool(columnName, (value == "true" || value == "1"));
            }
            else {
                record.setString(columnName, value);
            }
        }
        
        records.push_back(record);
    }
    
    // CRITICAL: Use setJSONArray for root-level arrays, then arrayToString()
    responseJson.setJSONArray(records);
    return responseJson;  // Use arrayToString() when returning this
}

// Usage example showing proper arrayToString usage:
HTTPResponse getUserListAPI(const HTTPRequest& req) {
    try {
        // Get data from database
        auto databaseResults = db->getUsers();
        
        // Serialize to JSON array
        JSONParser usersArray = serializeDatabaseResults(databaseResults);
        
        // For API responses, you might want to wrap the array
        JSONParser responseWrapper;
        responseWrapper.setBool("success", true);
        responseWrapper.setInt("total", databaseResults.size());
        
        // Add the array as a field in the wrapper object
        responseWrapper.setJSONArray("users", usersArray.getJSONArray());
        
        HTTPResponse response = responseOK(&req);
        // Use toString() for the wrapper object (it contains the array as a field)
        response.setBody(responseWrapper.toString());
        // Result: {"success":true,"total":5,"users":[{"id":1,"name":"John"},{"id":2,"name":"Jane"}]}
        
        return response;
    }
    catch (const std::exception& e) {
        JSONParser error;
        error.setString("error", "DatabaseError");
        error.setString("message", e.what());
        
        HTTPResponse response = responseInternalServerError(&req);
        response.setBody(error.toString());  // Use toString() for error objects
        return response;
    }
}

// Alternative: Direct array response (less common for APIs)
HTTPResponse getUserListDirectArray(const HTTPRequest& req) {
    try {
        auto databaseResults = db->getUsers();
        JSONParser usersArray = serializeDatabaseResults(databaseResults);
        
        HTTPResponse response = responseOK(&req);
        // Use arrayToString() for direct JSON array responses
        response.setBody(usersArray.arrayToString());
        // Result: [{"id":1,"name":"John"},{"id":2,"name":"Jane"}]
        
        return response;
    }
    catch (const std::exception& e) {
        // Error handling...
        return responseInternalServerError(&req);
    }
}
```

// Helper functions for type detection
bool isInteger(const std::string& str) {
    if (str.empty()) return false;
    
    size_t start = 0;
    if (str[0] == '-') start = 1;
    
    for (size_t i = start; i < str.length(); ++i) {
        if (!std::isdigit(str[i])) return false;
    }
    return true;
}

bool isFloat(const std::string& str) {
    if (str.empty()) return false;
    
    bool hasDecimal = false;
    size_t start = 0;
    if (str[0] == '-') start = 1;
    
    for (size_t i = start; i < str.length(); ++i) {
        if (str[i] == '.') {
            if (hasDecimal) return false;
            hasDecimal = true;
        }
        else if (!std::isdigit(str[i])) {
            return false;
        }
    }
    return hasDecimal;
}

bool isBoolean(const std::string& str) {
    return str == "true" || str == "false" || str == "0" || str == "1";
}
```

#### Error Handling
- **Constructor exceptions**: Throws `std::invalid_argument` for malformed JSON strings
- **Type mismatches**: Methods return default values (empty string, 0, false) for type mismatches
- **Missing keys**: Methods return default values when keys don't exist
- **File operations**: Return `nullptr` or `false` for file I/O failures

#### Migration from Manual JSON Construction
**Before (Manual string building):**
```cpp
std::string content = "[";
for(const auto& inner_vector : data) {
    content += "{";
    for(const auto& pair : inner_vector) {
        content += "\"" + pair.first + "\":\"" + pair.second + "\",";
    }
    content.pop_back(); // Remove last comma
    content += "},";
}
content.pop_back(); // Remove last comma
content += "]";
```

**After (Using geruest::JSONParser):**
```cpp
std::vector<JSONParser> jsonArray;
for(const auto& inner_vector : data) {
    JSONParser jsonObj;
    for(const auto& pair : inner_vector) {
        jsonObj.setString(pair.first, pair.second);
    }
    jsonArray.push_back(jsonObj);
}

JSONParser arrayJson;
arrayJson.setJSONArray(jsonArray);
return arrayJson.arrayToString();
```

**Benefits of Migration:**
- **Type safety**: No manual string escaping needed
- **Cleaner code**: Less error-prone than manual concatenation
- **Proper escaping**: Handles special characters automatically
- **Consistent formatting**: Framework ensures valid JSON output

## Complete Integration Example: HTTPRequest + JSONParser + HTTPResponse

Here's a comprehensive example showing how all three classes work together in a real-world API implementation:

### Complete CRUD API Implementation

```cpp
class UserAPI {
private:
    // Database interface (your implementation)
    DatabaseInterface* db;
    
    // Validation helper
    bool isValidEmail(const std::string& email) {
        return email.find("@") != std::string::npos && 
               email.find(".") != std::string::npos &&
               email.length() > 5;
    }
    
    bool isAuthenticated(const HTTPRequest& req) {
        if (!req.hasParam("api_key") || !req.hasParam("user_id")) {
            return false;
        }
        
        std::string apiKey = req.getParam("api_key");
        std::string userId = req.getParam("user_id");
        
        // Validate against database
        return db->validateApiKey(apiKey, userId);
    }

public:
    UserAPI(DatabaseInterface* database) : db(database) {}
    
    // Main router function
    HTTPResponse handleUserRequest(const HTTPRequest& req) {
        // Authentication for all endpoints except GET (public read)
        if (req.getMethod() != "GET" && !isAuthenticated(req)) {
            JSONParser error;
            error.setString("error", "Unauthorized");
            error.setString("message", "Valid API key and user ID required");
            
            HTTPResponse response = responseAuthRequired(&req);
            response.setBody(error.toString());
            return response;
        }
        
        // Route based on HTTP method and path
        if (req.getMethod() == "GET") {
            return handleGetUser(req);
        }
        else if (req.getMethod() == "POST") {
            return handleCreateUser(req);
        }
        else if (req.getMethod() == "PUT") {
            return handleUpdateUser(req);
        }
        else if (req.getMethod() == "DELETE") {
            return handleDeleteUser(req);
        }
        else {
            JSONParser error;
            error.setString("error", "MethodNotAllowed");
            error.setStringArray("allowed_methods", {"GET", "POST", "PUT", "DELETE"});
            
            HTTPResponse response = responseMethodNotAllowed(&req);
            response.setBody(error.toString());
            return response;
        }
    }
    
private:
    // GET /api/users/{id} or /api/users (list all)
    HTTPResponse handleGetUser(const HTTPRequest& req) {
        try {
            // Check if requesting specific user
            if (req.getPathLength() >= 3) {
                std::string userId = req.getPath(2);
                return getUserById(userId, req);
            }
            else {
                // List users with pagination
                return listUsers(req);
            }
        }
        catch (const std::exception& e) {
            JSONParser error;
            error.setString("error", "InternalServerError");
            error.setString("message", e.what());
            
            HTTPResponse response = responseInternalServerError(&req);
            response.setBody(error.toString());
            return response;
        }
    }
    
    HTTPResponse getUserById(const std::string& userId, const HTTPRequest& req) {
        auto userData = db->getUserById(userId);
        
        if (userData.empty()) {
            JSONParser error;
            error.setString("error", "UserNotFound");
            error.setString("message", "User with ID " + userId + " not found");
            
            HTTPResponse response = responseNotFound(&req);
            response.setBody(error.toString());
            return response;
        }
        
        // Convert database result to JSON
        JSONParser userJson = databaseRowToJSON(userData);
        
        // Wrap in response envelope
        JSONParser responseData;
        responseData.setBool("success", true);
        responseData.setJSON("user", userJson);
        
        HTTPResponse response = responseOK(&req);
        response.setBody(responseData.toString());
        return response;
    }
    
    HTTPResponse listUsers(const HTTPRequest& req) {
        // Extract pagination parameters
        int limit = 50;
        int offset = 0;
        
        if (req.hasParam("limit")) {
            try {
                limit = std::max(1, std::min(1000, std::stoi(req.getParam("limit"))));
            } catch (...) {}
        }
        
        if (req.hasParam("offset")) {
            try {
                offset = std::max(0, std::stoi(req.getParam("offset")));
            } catch (...) {}
        }
        
        // Search parameters
        std::string searchTerm = req.getParam("search");
        std::string sortBy = req.getParam("sort_by");
        std::string sortOrder = req.getParam("sort_order");
        
        // Query database
        auto users = db->getUsers(limit, offset, searchTerm, sortBy, sortOrder);
        int totalCount = db->getUserCount(searchTerm);
        
        // Convert to JSON array
        std::vector<JSONParser> userArray;
        for (const auto& userData : users) {
            userArray.push_back(databaseRowToJSON(userData));
        }
        
        // Build paginated response
        JSONParser responseData;
        responseData.setBool("success", true);
        responseData.setJSONArray(userArray);
        
        // Pagination metadata
        JSONParser pagination;
        pagination.setInt("total", totalCount);
        pagination.setInt("limit", limit);
        pagination.setInt("offset", offset);
        pagination.setBool("has_more", (offset + limit) < totalCount);
        responseData.setJSON("pagination", pagination);
        
        HTTPResponse response = responseOK(&req);
        response.setBody(responseData.arrayToString());
        return response;
    }
    
    // POST /api/users - Create new user
    HTTPResponse handleCreateUser(const HTTPRequest& req) {
        // Validate content type
        if (!req.hasHeader("content-type") || 
            req.getHeader("content-type").find("application/json") == std::string::npos) {
            
            JSONParser error;
            error.setString("error", "InvalidContentType");
            error.setString("message", "Content-Type must be application/json");
            
            HTTPResponse response = responseBadRequest(&req);
            response.setBody(error.toString());
            return response;
        }
        
        try {
            JSONParser requestData(req.getBody());
            
            // Validate required fields
            std::vector<std::string> validationErrors;
            
            std::string name = requestData.getString("name");
            std::string email = requestData.getString("email");
            int age = requestData.getInt("age");
            
            if (name.empty() || name.length() < 2) {
                validationErrors.push_back("Name must be at least 2 characters");
            }
            
            if (!isValidEmail(email)) {
                validationErrors.push_back("Valid email address is required");
            }
            
            if (age <= 0 || age > 150) {
                validationErrors.push_back("Age must be between 1 and 150");
            }
            
            // Check for email uniqueness
            if (db->emailExists(email)) {
                validationErrors.push_back("Email address already exists");
            }
            
            if (!validationErrors.empty()) {
                JSONParser error;
                error.setString("error", "ValidationError");
                error.setString("message", "One or more validation errors occurred");
                error.setStringArray("details", validationErrors);
                
                HTTPResponse response = responseBadRequest(&req);
                response.setBody(error.toString());
                return response;
            }
            
            // Extract optional fields
            bool isActive = requestData.getBool("active");
            std::string department = requestData.getString("department");
            std::vector<std::string> skills = requestData.getStringArray("skills");
            
            // Create user in database
            std::string userId = db->createUser(name, email, age, isActive, department, skills);
            
            // Retrieve created user
            auto userData = db->getUserById(userId);
            JSONParser userJson = databaseRowToJSON(userData);
            
            // Build success response
            JSONParser responseData;
            responseData.setBool("success", true);
            responseData.setString("message", "User created successfully");
            responseData.setString("user_id", userId);
            responseData.setJSON("user", userJson);
            
            HTTPResponse response = responseCreated(&req);
            response.setBody(responseData.toString());
            return response;
            
        }
        catch (const std::invalid_argument& e) {
            JSONParser error;
            error.setString("error", "InvalidJSON");
            error.setString("message", "Malformed JSON in request body");
            error.setString("details", e.what());
            
            HTTPResponse response = responseBadRequest(&req);
            response.setBody(error.toString());
            return response;
        }
        catch (const std::exception& e) {
            JSONParser error;
            error.setString("error", "InternalServerError");
            error.setString("message", "Failed to create user");
            error.setString("details", e.what());
            
            HTTPResponse response = responseInternalServerError(&req);
            response.setBody(error.toString());
            return response;
        }
    }
    
    // PUT /api/users/{id} - Update existing user
    HTTPResponse handleUpdateUser(const HTTPRequest& req) {
        if (req.getPathLength() < 3) {
            JSONParser error;
            error.setString("error", "BadRequest");
            error.setString("message", "User ID is required in path");
            
            HTTPResponse response = responseBadRequest(&req);
            response.setBody(error.toString());
            return response;
        }
        
        std::string userId = req.getPath(2);
        
        // Check if user exists
        if (!db->userExists(userId)) {
            JSONParser error;
            error.setString("error", "UserNotFound");
            error.setString("message", "User with ID " + userId + " not found");
            
            HTTPResponse response = responseNotFound(&req);
            response.setBody(error.toString());
            return response;
        }
        
        try {
            JSONParser requestData(req.getBody());
            
            // Get current user data
            auto currentData = db->getUserById(userId);
            
            // Update only provided fields (partial update)
            std::string name = requestData.hasKey("name") ? 
                requestData.getString("name") : currentData["name"];
            std::string email = requestData.hasKey("email") ? 
                requestData.getString("email") : currentData["email"];
            
            // Validate if email changed and new email is unique
            if (email != currentData["email"] && db->emailExists(email)) {
                JSONParser error;
                error.setString("error", "ConflictError");
                error.setString("message", "Email address already exists");
                
                HTTPResponse response = responseConflict(&req);
                response.setBody(error.toString());
                return response;
            }
            
            // Update user
            db->updateUser(userId, requestData);
            
            // Get updated user data
            auto updatedData = db->getUserById(userId);
            JSONParser userJson = databaseRowToJSON(updatedData);
            
            JSONParser responseData;
            responseData.setBool("success", true);
            responseData.setString("message", "User updated successfully");
            responseData.setJSON("user", userJson);
            
            HTTPResponse response = responseOK(&req);
            response.setBody(responseData.toString());
            return response;
            
        }
        catch (const std::exception& e) {
            JSONParser error;
            error.setString("error", "InternalServerError");
            error.setString("message", "Failed to update user");
            
            HTTPResponse response = responseInternalServerError(&req);
            response.setBody(error.toString());
            return response;
        }
    }
    
    // DELETE /api/users/{id} - Delete user
    HTTPResponse handleDeleteUser(const HTTPRequest& req) {
        if (req.getPathLength() < 3) {
            JSONParser error;
            error.setString("error", "BadRequest");
            error.setString("message", "User ID is required in path");
            
            HTTPResponse response = responseBadRequest(&req);
            response.setBody(error.toString());
            return response;
        }
        
        std::string userId = req.getPath(2);
        
        if (!db->userExists(userId)) {
            JSONParser error;
            error.setString("error", "UserNotFound");
            error.setString("message", "User with ID " + userId + " not found");
            
            HTTPResponse response = responseNotFound(&req);
            response.setBody(error.toString());
            return response;
        }
        
        try {
            db->deleteUser(userId);
            
            JSONParser responseData;
            responseData.setBool("success", true);
            responseData.setString("message", "User deleted successfully");
            responseData.setString("deleted_user_id", userId);
            
            HTTPResponse response = responseOK(&req);
            response.setBody(responseData.toString());
            return response;
            
        }
        catch (const std::exception& e) {
            JSONParser error;
            error.setString("error", "InternalServerError");
            error.setString("message", "Failed to delete user");
            
            HTTPResponse response = responseInternalServerError(&req);
            response.setBody(error.toString());
            return response;
        }
    }
    
    // Helper: Convert database row to JSON
    JSONParser databaseRowToJSON(const std::map<std::string, std::string>& row) {
        JSONParser json;
        
        for (const auto& pair : row) {
            const std::string& key = pair.first;
            const std::string& value = pair.second;
            
            // Smart type conversion based on key patterns or value format
            if (key.find("_id") != std::string::npos || key == "id") {
                json.setString(key, value);  // IDs as strings
            }
            else if (key == "age" || key.find("_count") != std::string::npos) {
                json.setInt(key, value.empty() ? 0 : std::stoi(value));
            }
            else if (key.find("is_") == 0 || key == "active" || key == "enabled") {
                json.setBool(key, value == "true" || value == "1");
            }
            else {
                json.setString(key, value);
            }
        }
        
        return json;
    }
};
```

### Usage in Main Server
```cpp
#include <Geruest.hpp>
using namespace geruest;

int main() {
    try {
        // Create server instance
        Geruest server(8080);
        
        // Initialize database
        DatabaseInterface db("connection_string");
        UserAPI userAPI(&db);
        
        // Register user API routes
        server.addRoute("/api/users", [&userAPI](const HTTPRequest& req) {
            return userAPI.handleUserRequest(req);
        });
        
        server.addRoute("/api/users/*", [&userAPI](const HTTPRequest& req) {
            return userAPI.handleUserRequest(req);
        });
        
        // Health check endpoint
        server.addRoute("/api/health", [](const HTTPRequest& req) {
            JSONParser healthData;
            healthData.setString("status", "healthy");
            healthData.setString("timestamp", getCurrentTimestamp());
            healthData.setString("version", "1.0.0");
            
            HTTPResponse response = responseOK(&req);
            response.setBody(healthData.toString());
            return response;
        });
        
        // Start server
        std::cout << "Server starting on port 8080..." << std::endl;
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

This comprehensive example demonstrates:
- **HTTPRequest**: Parameter extraction, path parsing, header access, body content parsing
- **JSONParser**: Request parsing, response building, error handling, data serialization
- **HTTPResponse**: Appropriate status codes, automatic CORS, consistent error responses
- **Integration**: How all three classes work together for a complete API implementation
- **Best Practices**: Validation, error handling, authentication, pagination, partial updates

### HTTP Response Helpers
The Geruest framework provides standardized HTTP response helpers for consistent API responses with automatic CORS handling:

```cpp
// Success responses
responseOK(const HTTPRequest* request = nullptr);           // 200 OK
responseCreated(const HTTPRequest* request = nullptr);      // 201 Created
responseAccepted(const HTTPRequest* request = nullptr);     // 202 Accepted
responseNoContent(const HTTPRequest* request = nullptr);    // 204 No Content

// Client error responses  
responseBadRequest(const HTTPRequest* request = nullptr);   // 400 Bad Request
responseAuthRequired(const HTTPRequest* request = nullptr); // 401 Unauthorized
responseForbidden(const HTTPRequest* request = nullptr);    // 403 Forbidden
responseNotFound(const HTTPRequest* request = nullptr);     // 404 Not Found
responseMethodNotAllowed(const HTTPRequest* request = nullptr); // 405 Method Not Allowed
responseConflict(const HTTPRequest* request = nullptr);     // 409 Conflict

// Server error responses
responseInternalServerError(const HTTPRequest* request = nullptr); // 500 Internal Server Error
```

#### Automatic CORS Header Management
**Critical**: When using framework response helpers, CORS headers are automatically handled:

- **With request parameter**: Pass `&request` to automatically set CORS headers based on request origin
- **Without request parameter**: No CORS headers are set (for internal responses)
- **Manual headers avoided**: Never manually set `Access-Control-*` headers when using framework helpers

```cpp
// Correct: Framework handles CORS automatically
HTTPResponse response = responseOK(&apiRequest);
response.setBody("{\"success\": true}");

// Wrong: Manual CORS header setting (redundant with framework)
HTTPResponse response = responseOK(&apiRequest);
response.setHeader("Access-Control-Allow-Origin", "*");  // Don't do this!
```

#### Generic vs Specific Response Functions
The framework provides both **generic** and **specific** response creation methods:

**Generic Functions** (recommended):
- `responseOK()`, `responseBadRequest()`, `responseNotFound()`, etc.
- Handle common HTTP status codes with proper defaults
- Automatic CORS when request is provided
- Content-Type automatically set to "application/json"
- Eliminates ~6 lines of boilerplate header setting per response

**Manual Construction** (avoid unless necessary):
```cpp
// Only use for custom status codes not covered by helpers
HTTPResponse response("418 I'm a teapot");
response.setHeader("Content-Type", "application/json");
// Manual CORS handling required if cross-origin support needed
if (request.hasOrigin()) {
    response.setHeader("Access-Control-Allow-Origin", request.getOrigin());
    response.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, DELETE, PUT");
    response.setHeader("Access-Control-Allow-Headers", "Origin, Content-Type, Accept, Authorization");
    response.setHeader("Access-Control-Allow-Credentials", "true");
}
```

**Comparison Benefits:**
- **Generic**: `HTTPResponse response = responseOK(&req);` (1 line)
- **Manual**: `HTTPResponse response("200 OK"); + 5-6 header lines` (6-7 lines)
- **Generic**: Automatic CORS detection and handling
- **Manual**: Must manually check origin and set headers
- **Generic**: Consistent header patterns across all responses
- **Manual**: Risk of inconsistent or missing headers

### Request Handler Pattern
All API handlers should follow this standardized pattern with proper framework response usage:

```cpp
HTTPResponse handleRequest(const HTTPRequest& req) {
    // Authentication check (if required)
    if (!isAuthenticated(req))
        return responseAuthRequired(&req);  // Framework handles CORS automatically
    
    // Input validation
    if (!req.hasParam("required_param"))
        return responseBadRequest(&req);
    
    // Business logic
    try {
        // ... perform operations ...
        
        // Success response with automatic CORS
        HTTPResponse response = responseOK(&req);
        response.setBody("{\"success\": true}");
        return response;
        
    } catch (const std::exception& e) {
        // Error response with automatic CORS
        HTTPResponse response = responseInternalServerError(&req);
        response.setBody("{\"error\": \"Internal server error\"}");
        return response;
    }
}
```

**Authentication Implementation Pattern:**
```cpp
// Consistent authentication validation
inline bool isAuthenticated(const HTTPRequest& request) {
    if (!request.hasParam("key") || !request.hasParam("user_id")) {
        return false;
    }
    
    std::string key = request.getParam("key");
    std::string user_id = request.getParam("user_id");
    
    // Use same validation logic as login system
    PostgreSQLInterface db(getConnectionString());
    std::string sqlRequest = "SELECT * FROM " + API_KEY_TABLE + " WHERE key = '" + key + "' AND user_id = '" + user_id + "';";
    PostgreSQLTable result = db.read(sqlRequest);
    
    return !result.empty();
}
```

**Key Points:**
- Always pass `&request` parameter for automatic CORS handling
- Use appropriate framework helper for each HTTP status code
- Set body content after creating the response
- Framework automatically sets `Content-Type: application/json`
- Ensure authentication validation uses consistent parameter names across all APIs

### CORS (Cross-Origin Resource Sharing) Handling

The Geruest framework provides **automatic CORS handling** when using response helpers:

#### Automatic CORS Detection
```cpp
// Framework automatically detects if request has an origin header
HTTPResponse response = responseOK(&apiRequest);  // CORS headers added automatically
// vs
HTTPResponse response = responseOK();  // No CORS headers (internal use)
```

#### CORS Headers Set Automatically
When a request parameter is passed, the framework automatically sets:
- `Access-Control-Allow-Origin`: Mirrors the request's origin header
- `Access-Control-Allow-Methods`: "GET, POST, OPTIONS, DELETE, PUT"
- `Access-Control-Allow-Headers`: "Origin, Content-Type, Accept, Authorization"
- `Access-Control-Allow-Credentials`: "true"

#### CORS Best Practices
- **Always pass `&request`** to response helpers for API endpoints
- **Never manually set CORS headers** when using framework helpers
- **Framework handles origin validation** automatically
- **Preflight requests** are handled by the framework

## Website Template Processing

### Critical Website Structure Requirements
For Geruest websites to function correctly, files must be organized in specific folder structures:

#### Required Folder Structure
```
website/
├── html/                   # HTML templates
├── assets/
│   ├── css/                # CSS source files
│   ├── docs/               # Documentations like sitemap
│   ├── images/             # Image assets
│   ├── js/                 # JavaScript source files
│   └── translations/       # Translation JSON files
├── components/             # Reusable HTML components
└── files_maps/             # Asset merging configuration
    ├── css_file_map.json   # CSS file merging rules
    └── js_file_map.json    # JS file merging rules
```

### Server-Side Template Processing

#### Translation System
**Critical**: Geruest processes a unique bracket-based translation system **server-side** before sending HTML to the browser:

```html
<!-- Translation placeholders (processed server-side) -->
<title>[/assets/translations/page.json:title]</title>
<p>[/assets/translations/page.json:description]</p>
```

- Server processes `[path/file.json:key]` syntax
- Injects corresponding text based on detected/requested language
- Translations must exist in JSON files before use in HTML

#### Component Injection
HTML components are injected using `{...}` syntax:
```html
<!-- Component injection (processed server-side) -->
{/components/header.html}
<main>
    <!-- Page content -->
</main>
{/components/footer.html}
```

### Asset Management & File Merging

#### Image Path Conventions
**Critical**: Geruest automatically serves images from `/assets/images/` directory with simplified path references:

**✅ Correct Path Usage (Absolute Paths Recommended):**
```html
<!-- In HTML templates - use absolute paths for reliability -->
<img src="/icons/moon.svg" alt="Dark mode toggle">
<img src="/logos/company.png" alt="Company logo">
```

```javascript
// In JavaScript code - absolute paths prevent issues in subdirectories
themeIcon.src = '/icons/sun.svg';
profileImage.src = '/avatars/user.jpg';
```

**⚠️ Relative Paths (Use with caution):**
```html
<!-- Relative paths work from root but can cause issues in subdirectories -->
<img src="icons/moon.svg" alt="Dark mode toggle">
<img src="logos/company.png" alt="Company logo">
```

**❌ Incorrect Path Usage:**
```html
<!-- Don't include the full assets/images path -->
<img src="assets/images/icons/moon.svg" alt="Dark mode toggle">
```

**Path Resolution:**
- `/icons/moon.svg` → automatically resolves to `/assets/images/icons/moon.svg`
- `icons/moon.svg` → resolves relative to current page location (can cause issues in subdirectories)
- Framework handles the `/assets/images/` prefix automatically
- **Best Practice**: Use absolute paths (`/icons/...`) for components and JavaScript that may be used from various page locations

**Navigation Path Issues:**
- Pages in subdirectories (like `/devices/devices`) resolve relative paths from their current location
- Relative `icons/moon.svg` from `/devices/devices` resolves to `/devices/icons/moon.svg` (incorrect)
- Absolute `/icons/moon.svg` always resolves correctly regardless of page location

**Common Image Directory Structure:**
```
website/assets/images/
├── icons/           # UI icons (moon.svg, sun.svg, etc.)
├── logos/           # Company and brand logos
├── avatars/         # User profile images
└── backgrounds/     # Background images
```

#### CSS/JS File Merging
**Critical**: Geruest automatically merges CSS and JS files **server-side** based on filename configuration:

- Files are mapped using the filename without extension as the key
- Each requested file can have multiple source files merged into it
- Merging rules defined in `website/files_maps/css_file_map.json` and `js_file_map.json`
- Files are loaded in numerical order (1, 2, 3, etc.)

#### File Maps Configuration
**Filename-Based Mapping Pattern:**

Example `css_file_map.json`:
```json
{
  "index": {
    "1": "/tailwind.css"
  },
  "dashboard": {
    "1": "/tailwind.css",
    "2": "/dashboard.css"
  }
}
```

Example `js_file_map.json`:
```json
{
  "index": {
    "1": "/main.js",
    "2": "/index.js"
  },
  "dashboard": {
    "1": "/main.js",
    "2": "/dashboard.js"
  }
}
```

**Key Points:**
- **Filename without extension** is the top-level key (e.g., "index", "dashboard")
- **Numerical keys** ("1", "2", "3") define load order
- **File paths** start with "/" and reference files in `/assets/css/` or `/assets/js/`
- **main.js** is typically included as file "1" on all pages for shared functionality

#### Shared Functionality Pattern
- `main.js` typically contains functions needed across multiple pages
- Gets merged into each requested JS file as source file "1"
- Page-specific files (like `/index.js`, `/dashboard.js`) are added as file "2", "3", etc.
- Allows code reuse while maintaining file-specific bundling

#### Practical Example
For a page with `<script src="index.js"></script>`:
1. Framework looks up "index" (filename without extension) in `js_file_map.json`
2. Merges files in numerical order: `/main.js` (file "1") + `/index.js` (file "2")
3. Serves the merged result when `index.js` is requested
4. Same pattern applies for CSS files

**HTML File References:**
```html
<link rel="stylesheet" href="index.css">
<script src="index.js"></script>
```

**File Map Keys (without extension):**
- `index.css` → looks up "index" in `css_file_map.json`
- `index.js` → looks up "index" in `js_file_map.json`

**Actual File Merging:**
- `index.css` → serves merged CSS from file map "index" entries
- `index.js` → serves merged JS from file map "index" entries (/main.js + /index.js)

### Multi-Language Support

#### URL Pattern
- URLs should follow pattern: `/language/page` (e.g., `/en/dashboard`, `/de/profile`, `/fr/dashboard`)
- **Language codes must be exactly 2 characters long** (ISO 639-1 format)
- **Supported languages**: Any languages defined in the translation JSON files
- Language detection from URL path segments using strict 2-character validation
- Automatic language-based template processing based on detected language code
- URLs without language prefix or with invalid language codes fall back to default language

#### Language Code Validation
- **Valid examples**: `/de/`, `/en/dashboard`, `/fr/profile`, `/es/settings`, `/it/users`
- **Invalid examples**: `/deutsch/`, `/english/`, `/fra/`, `/español/` (not 2 characters)
- **Language availability**: Must exist as a key in the translation JSON files
- **Fallback behavior**: Invalid or missing language codes default to English (`en`)
- **URL structure**: Language-prefixed URLs must include trailing slash for root paths (`/de/`, not `/de`)

#### Client-Side Translation Loading
For JavaScript operations, load translations separately in each JS file:
```javascript
const translations = { 
    en: { key: "English text" }, 
    de: { key: "German text" }, 
    fr: { key: "French text" } 
};

function t(key, vars = {}) { 
    // Get translation for current language
    // Handle variable substitution
}
```

## Frontend JavaScript Patterns

### Authentication Parameter Handling
When making API calls that require authentication, explicitly include parameters:

```javascript
// Extract authentication from cookies
const apiKey = getCookie('key');
const userId = getCookie('user_id');

if (!apiKey || !userId) {
    throw new Error('Authentication required');
}

// Include in API calls as query parameters
const response = await fetch(`${apiUrl}/api-endpoint?key=${encodeURIComponent(apiKey)}&user_id=${encodeURIComponent(userId)}`, {
    method: 'GET',
    credentials: 'include',
    headers: {
        'Content-Type': 'application/json',
    }
});
```

### Error Handling Patterns
```javascript
if (!response.ok) {
    if (response.status === 401) {
        throw new Error('Authentication failed. Please log in again.');
    }
    const errorData = await response.json().catch(() => ({ message: 'Unknown error' }));
    throw new Error(errorData.message || 'Request failed');
}
```

### Clean URL Navigation
```javascript
// Use clean URLs without .html extensions
window.location.href = '/devices/devices';
window.location.href = '/devices/addDevice';

// Language-aware redirects
function redirectToLanguageAwarePage(path) {
    const currentLang = getLanguageFromURL();
    window.location.href = `/${currentLang}${path}`;
}
```

## Request Parameter Handling

### Standard Parameter Patterns
- **Pagination**: Use `limit` and `offset` parameters (framework provides defaults)
- **Search**: Use `search` parameter for filtering
- **CORS**: Framework automatically handles CORS headers when configured

### Request Parameter Access
```cpp
// Get request parameters
std::string value = req.getParam("parameter_name");
```

## Build Integration

### Development Workflow
The framework integrates with standard build tools:
- C++ compilation with CMake
- Frontend asset building (CSS/JS processing)
- Automatic template processing during server runtime

### Framework Dependencies
- Requires access to Geruest library (may need SSH key for private repo access)
- PostgreSQL integration available
- SSL/HTTPS support for external API calls

## Best Practices

### File Organization
- Keep HTML templates in `website/html/`
- Organize assets by type in `website/assets/`
- Use components for reusable HTML elements
- Configure file merging for optimal performance

### Response Consistency
- Always use framework response helpers (`responseOK()`, `responseBadRequest()`, etc.)
- Pass `&request` parameter to enable automatic CORS handling
- Never manually set CORS headers when using framework helpers
- Follow RESTful patterns for API endpoints
- Prefer generic response functions over manual HTTP response construction

### Template Processing
- Ensure translation JSON files exist before referencing in HTML
- Test component injection paths are correct
- Verify file map configurations for proper asset merging

## Framework Limitations & Gotchas

- **Server-side processing**: All template features (translations, components) are processed server-side before delivery
- **File structure dependency**: Website functionality depends on exact folder structure
- **Build dependencies**: May require specific SSH keys or access tokens for private framework repositories
- **Asset merging**: Changes to file maps require server restart to take effect
- **Path resolution**: Relative paths in components can resolve incorrectly when used from subdirectories
- **Authentication consistency**: API authentication requires explicit parameter passing; relying solely on cookies may not work consistently across all endpoints
- **URL clean-up**: Framework serves HTML files without extensions, but navigation must be updated to use clean URLs
- **Parameter validation**: Authentication functions must validate both required parameters (`key` AND `user_id`) for security