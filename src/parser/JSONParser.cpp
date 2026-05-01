#include "JSONParser.hpp"
#include <cctype>
#include <charconv>
#include <iostream>
#include <stdexcept>
#include "security/Security.hpp"

namespace geruest {

namespace {

[[nodiscard]] bool isJsonNumberLiteral(const std::string& s) {
    // RFC 8259 number grammar:
    // -?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?
    if (s.empty()) return false;

    size_t i = 0;
    const size_t n = s.size();

    if (s[i] == '-') {
        ++i;
        if (i >= n) return false;
    }

    if (s[i] == '0') {
        ++i;
        if (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    } else if (std::isdigit(static_cast<unsigned char>(s[i]))) {
        if (s[i] < '1' || s[i] > '9') return false;
        ++i;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    } else {
        return false;
    }

    if (i < n && s[i] == '.') {
        ++i;
        if (i >= n) return false;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }

    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i >= n) return false;
        if (s[i] == '+' || s[i] == '-') {
            ++i;
            if (i >= n) return false;
        }
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }

    return i == n;
}

[[nodiscard]] size_t escapedJsonSize(const std::string& input) {
    size_t out = 0;
    for (unsigned char c : input) {
        switch (c) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                out += 2;
                break;
            default:
                out += (c < 0x20U) ? 6 : 1;
                break;
        }
    }
    return out;
}

}  // namespace

JSONParser::JSONParser(const std::string &input) : _ownedStorage(input), _view(_ownedStorage), jp(0) {
    parseJSON();
}

JSONParser::JSONParser(std::string_view json, std::shared_ptr<const std::string> lifetime)
    : _lifetimeBacking(std::move(lifetime)), _view(json), jp(0) {
    parseJSON();
}

JSONParser::JSONParser(std::map<std::string, std::string> initialData) : data(std::move(initialData)) {}

bool JSONParser::isWhiteSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string JSONParser::readKey() {
    while (jp < _view.length() && isWhiteSpace(_view[jp])) {
        jp++;
    }
    if (jp >= _view.length() || _view[jp] != '"') {
        return "";
    }
    jp++;
    std::string key;
    bool escape = false;
    while (jp < _view.length()) {
        if (escape) {
            key += _view[jp];
            escape = false;
            jp++;
        } else if (_view[jp] == '\\') {
            escape = true;
            jp++;
        } else if (_view[jp] == '"') {
            jp++;
            break;
        } else {
            key += _view[jp];
            jp++;
        }
    }
    return key;
}

std::string JSONParser::readString() {
    while (jp < _view.length() && isWhiteSpace(_view[jp])) {
        jp++;
    }
    if (jp >= _view.length() || _view[jp] != '"') {
        return "";
    }
    jp++;
    std::string value;
    bool escape = false;
    while (jp < _view.length()) {
        if (escape) {
            value += _view[jp];
            escape = false;
            jp++;
        } else if (_view[jp] == '\\') {
            escape = true;
            jp++;
        } else if (_view[jp] == '"') {
            jp++;
            break;
        } else {
            value += _view[jp];
            jp++;
        }
    }
    return value;
}

std::string JSONParser::readNumber() {
    while (jp < _view.length() && isWhiteSpace(_view[jp])) {
        jp++;
    }
    std::string number;
    if (jp < _view.length() && _view[jp] == '-') {
        number += _view[jp];
        jp++;
    }
    while (jp < _view.length() && 
           (std::isdigit(_view[jp]) || _view[jp] == '.')) {
        number += _view[jp];
        jp++;
    }
    return number;
}

std::map<std::string, std::string> JSONParser::readObject() {
    std::map<std::string, std::string> obj;
    return obj;
}

std::vector<std::string> JSONParser::readArray() {
    std::vector<std::string> arr;
    return arr;
}

std::string JSONParser::readNestedObject() {
    int braceCount = 0;
    size_t start = jp;
    bool inString = false;
    bool escape = false;
    
    while (jp < _view.length()) {
        char c = _view[jp];
        if (escape) {
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            inString = !inString;
        } else if (!inString) {
            if (c == '{') {
                braceCount++;
            } else if (c == '}') {
                braceCount--;
                if (braceCount == 0) {
                    jp++; // Move past the closing brace
                    break;
                }
            }
        }
        jp++;
    }
    
    return std::string(_view.substr(start, jp - start));
}

std::string JSONParser::readNestedArray() {
    int bracketCount = 0;
    size_t start = jp;
    bool inString = false;
    bool escape = false;

    while (jp < _view.length()) {
        char c = _view[jp];
        if (escape) {
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            inString = !inString;
        } else if (!inString) {
            if (c == '[') {
                bracketCount++;
            } else if (c == ']') {
                bracketCount--;
                if (bracketCount == 0) {
                    jp++; // Move past the closing bracket
                    break;
                }
            }
        }
        jp++;
    }
    return std::string(_view.substr(start, jp - start));
}

std::string JSONParser::readData() {
    while (jp < _view.length() && isWhiteSpace(_view[jp])) {
        jp++;
    }
    if (jp >= _view.length()) {
        return "";
    }
    char c = _view[jp];
    if (c == '"') {
        return readString();
    } else if (std::isdigit(c) || c == '-') {
        return readNumber();
    } else if (jp + 4 <= _view.size() && _view.substr(jp, 4) == "true") {
        jp += 4;
        return "true";
    } else if (jp + 5 <= _view.size() && _view.substr(jp, 5) == "false") {
        jp += 5;
        return "false";
    } else if (jp + 4 <= _view.size() && _view.substr(jp, 4) == "null") {
        jp += 4;
        return "null";
    } else if (c == '{') {
        return readNestedObject();
    } else if (c == '[') {
        return readNestedArray();
    }
    return "";
}

void JSONParser::parseArray() {
    arrayData.clear();
    jp = 0;
    
    // Skip whitespace
    while (jp < _view.length() && isWhiteSpace(_view[jp])) {
        jp++;
    }
    
    // Must start with '['
    if (jp >= _view.length() || _view[jp] != '[') {
        return;
    }
    jp++;
    
    // Parse array elements
    while (jp < _view.length()) {
        // Skip whitespace
        while (jp < _view.length() && isWhiteSpace(_view[jp])) {
            jp++;
        }
        
        // Check for end of array
        if (jp < _view.length() && _view[jp] == ']') {
            jp++;
            break;
        }
        
        // Read the element (should be an object)
        std::string element = readData();
        
        // If it's an object, parse it and add to arrayData
        if (!element.empty()) {
            JSONParser parser(element);
            arrayData.push_back(parser);
        }
        
        // Skip whitespace
        while (jp < _view.length() && isWhiteSpace(_view[jp])) {
            jp++;
        }
        
        // Check for comma or end of array
        if (jp < _view.length() && _view[jp] == ',') {
            jp++;
        } else if (jp < _view.length() && _view[jp] == ']') {
            jp++;
            break;
        }
    }
}

void JSONParser::parseJSON() {
    data.clear();
    keys.clear();
    jp = 0;
    
    // Skip leading whitespace
    while (jp < _view.length() && isWhiteSpace(_view[jp])) {
        jp++;
    }
    
    // Check if this is an array or object
    if (jp < _view.length() && _view[jp] == '[') {
        // This is an array, parse it
        parseArray();
        return;
    }
    
    // This should be an object
    if (jp >= _view.length() || _view[jp] != '{') {
        return;
    }
    jp++;
    
    while (jp < _view.length()) {
        while (jp < _view.length() && isWhiteSpace(_view[jp])) {
            jp++;
        }
        if (jp < _view.length() && _view[jp] == '}') {
            jp++;
            break;
        }
        
        std::string key = readKey();
        if (key.empty()) break;
        keys.push_back(key);
        
        while (jp < _view.length() && isWhiteSpace(_view[jp])) {
            jp++;
        }
        if (jp >= _view.length() || _view[jp] != ':') {
            break;
        }
        jp++;
        
        std::string value = readData();
        data[key] = value;
        
        while (jp < _view.length() && isWhiteSpace(_view[jp])) {
            jp++;
        }
        if (jp < _view.length() && _view[jp] == ',') {
            jp++;
        } else if (jp < _view.length() && _view[jp] == '}') {
            jp++;
            break;
        }
    }
}

std::string JSONParser::stringToString(const std::string &val) const {
    return "\"" + Security::escapeJson(val) + "\"";
}

std::string JSONParser::getString(const std::string &key) {
    if (data.find(key) == data.end()) {
        return "";
    }
    return data[key];
}

int JSONParser::getInt(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0;
    }
    try {
        return std::stoi(data[key]);
    } catch (const std::exception&) {
        return 0;
    }
}

short JSONParser::getShort(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0;
    }
    try {
        return static_cast<short>(std::stoi(data[key]));
    } catch (const std::exception&) {
        return 0;
    }
}

bool JSONParser::getBool(const std::string &key) {
    if (data.find(key) == data.end()) {
        return false;
    }
    const std::string& value = data[key];
    return value == "true" || value == "1";
}

float JSONParser::getFloat(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0.0f;
    }
    try {
        return std::stof(data[key]);
    } catch (const std::exception&) {
        return 0.0f;
    }
}

double JSONParser::getDouble(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0.0;
    }
    try {
        return std::stod(data[key]);
    } catch (const std::exception&) {
        return 0.0;
    }
}

long JSONParser::getLong(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0L;
    }
    try {
        return std::stol(data[key]);
    } catch (const std::exception&) {
        return 0L;
    }
}

long long JSONParser::getLongLong(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0LL;
    }
    try {
        return std::stoll(data[key]);
    } catch (const std::exception&) {
        return 0LL;
    }
}

long double JSONParser::getLongDouble(const std::string &key) {
    if (data.find(key) == data.end()) {
        return 0.0L;
    }
    try {
        return std::stold(data[key]);
    } catch (const std::exception&) {
        return 0.0L;
    }
}

JSONParser JSONParser::getObject(const std::string &key) {
    if (data.find(key) == data.end()) {
        return JSONParser();
    }
    return JSONParser(data[key]);
}

// Helper function to parse array string into individual elements
std::vector<std::string> JSONParser::parseArrayString(const std::string& arrayStr) {
    std::vector<std::string> result;
    
    if (arrayStr.empty() || arrayStr[0] != '[') {
        return result;
    }
    
    size_t pos = 1; // Skip opening '['
    bool inString = false;
    bool escape = false;
    int bracketDepth = 0;
    int braceDepth = 0;
    std::string current;
    
    while (pos < arrayStr.length()) {
        char c = arrayStr[pos];
        
        if (escape) {
            current += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
            current += c;
        } else if (c == '"') {
            inString = !inString;
            current += c;
        } else if (!inString) {
            if (c == '[') {
                bracketDepth++;
                current += c;
            } else if (c == ']') {
                if (bracketDepth > 0) {
                    bracketDepth--;
                    current += c;
                } else {
                    // End of array
                    if (!current.empty()) {
                        // Trim whitespace
                        size_t start = current.find_first_not_of(" \t\n\r");
                        size_t end = current.find_last_not_of(" \t\n\r");
                        if (start != std::string::npos) {
                            result.push_back(current.substr(start, end - start + 1));
                        }
                    }
                    break;
                }
            } else if (c == '{') {
                braceDepth++;
                current += c;
            } else if (c == '}') {
                braceDepth--;
                current += c;
            } else if (c == ',' && bracketDepth == 0 && braceDepth == 0) {
                // Element separator
                if (!current.empty()) {
                    // Trim whitespace
                    size_t start = current.find_first_not_of(" \t\n\r");
                    size_t end = current.find_last_not_of(" \t\n\r");
                    if (start != std::string::npos) {
                        result.push_back(current.substr(start, end - start + 1));
                    }
                }
                current.clear();
            } else {
                current += c;
            }
        } else {
            current += c;
        }
        pos++;
    }
    
    return result;
}

// Helper function to extract string value (remove quotes)
std::string JSONParser::extractString(const std::string& str) {
    if (str.length() >= 2 && str[0] == '"' && str[str.length()-1] == '"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

std::vector<std::string> JSONParser::getStringArray(const std::string &key) {
    std::vector<std::string> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        result.push_back(extractString(elem));
    }
    
    return result;
}

std::vector<short> JSONParser::getShortArray(const std::string &key) {
    std::vector<short> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(static_cast<short>(std::stoi(elem)));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<int> JSONParser::getIntArray(const std::string &key) {
    std::vector<int> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(std::stoi(elem));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<long> JSONParser::getLongArray(const std::string &key) {
    std::vector<long> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(std::stol(elem));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<long long> JSONParser::getLongLongArray(const std::string &key) {
    std::vector<long long> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(std::stoll(elem));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<bool> JSONParser::getBoolArray(const std::string &key) {
    std::vector<bool> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        std::string lower = elem;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        result.push_back(lower == "true" || lower == "1");
    }
    
    return result;
}

std::vector<float> JSONParser::getFloatArray(const std::string &key) {
    std::vector<float> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(std::stof(elem));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<double> JSONParser::getDoubleArray(const std::string &key) {
    std::vector<double> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(std::stod(elem));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<long double> JSONParser::getLongDoubleArray(const std::string &key) {
    std::vector<long double> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        try {
            result.push_back(std::stold(elem));
        } catch (...) {
            // Skip invalid elements
        }
    }
    
    return result;
}

std::vector<JSONParser> JSONParser::getArrayOfJSON(const std::string &key) {
    std::vector<JSONParser> result;
    
    if (data.find(key) == data.end()) {
        return result;
    }
    
    std::vector<std::string> elements = parseArrayString(data[key]);
    for (const auto& elem : elements) {
        // Each element should be a JSON object or a valid JSON string
        try {
            result.push_back(JSONParser(elem));
        } catch (...) {
            // Skip invalid JSON elements
        }
    }
    
    return result;
}

std::vector<JSONParser> JSONParser::getJSONArray() {
    return arrayData;
}

void JSONParser::setString(const std::string &key, const std::string &value) {
    data[key] = value;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setInt(const std::string &key, int value) {
    data[key] = std::to_string(value);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setShort(const std::string &key, short value) {
    data[key] = std::to_string(value);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setBool(const std::string &key, bool value) {
    data[key] = value ? "true" : "false";
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setFloat(const std::string &key, float value) {
    data[key] = std::to_string(value);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setDouble(const std::string &key, double value) {
    data[key] = std::to_string(value);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setLong(const std::string &key, long value) {
    char buffer[32];
    auto conv = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (conv.ec == std::errc()) {
        data[key].assign(buffer, static_cast<size_t>(conv.ptr - buffer));
    } else {
        data[key] = std::to_string(value);
    }
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setLongLong(const std::string &key, long long value) {
    data[key] = std::to_string(value);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setLongDouble(const std::string &key, long double value) {
    data[key] = std::to_string(value);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setJSON(const std::string &key, const JSONParser &value) {
    data[key] = value.toString();
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setStringArray(const std::string &key, const std::vector<std::string> &value) {
    size_t estimatedSize = 2;  // []
    if (!value.empty()) {
        estimatedSize += value.size() - 1;  // commas
    }
    for (const std::string& item : value) {
        estimatedSize += 2 + escapedJsonSize(item);  // quotes + escaped content
    }

    std::string arrayStr;
    arrayStr.reserve(estimatedSize);
    arrayStr.push_back('[');
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr.push_back(',');
        arrayStr.push_back('"');
        arrayStr += Security::escapeJson(value[i]);
        arrayStr.push_back('"');
    }
    arrayStr.push_back(']');
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setShortArray(const std::string &key, const std::vector<short> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setIntArray(const std::string &key, const std::vector<int> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setLongArray(const std::string &key, const std::vector<long> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setLongLongArray(const std::string &key, const std::vector<long long> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setBoolArray(const std::string &key, const std::vector<bool> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += value[i] ? "true" : "false";
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setFloatArray(const std::string &key, const std::vector<float> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setDoubleArray(const std::string &key, const std::vector<double> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setLongDoubleArray(const std::string &key, const std::vector<long double> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += std::to_string(value[i]);
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setArrayOfJSON(const std::string &key, const std::vector<JSONParser> &value) {
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += value[i].toString();
    }
    arrayStr += "]";
    data[key] = arrayStr;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void JSONParser::setJSONArray(const std::vector<JSONParser> &value) {
    arrayData = value;
}

void JSONParser::addArrayOfJSON(const std::string &key, const std::vector<JSONParser> &value) {
    setArrayOfJSON(key, value);
}

void JSONParser::addJSONToArray(const JSONParser &value) {
    arrayData.push_back(value);
}

void JSONParser::removeKey(const std::string &key) {
    data.erase(key);
    keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
}

std::string JSONParser::toString() const {
    if (data.empty() && !arrayData.empty()) {
        return arrayToString();
    }

    size_t estimatedSize = 2;  // {}
    for (const auto& key : keys) {
        auto it = data.find(key);
        if (it == data.end()) {
            continue;
        }
        const std::string& value = it->second;
        estimatedSize += key.size() + 4;  // "key":

        if (!value.empty() && (value[0] == '{' || value[0] == '[')) {
            estimatedSize += value.size();
        } else if (value == "true" || value == "false" || value == "null" || isJsonNumberLiteral(value)) {
            estimatedSize += value.size();
        } else {
            estimatedSize += escapedJsonSize(value) + 2;  // quoted string
        }
    }

    std::string result;
    result.reserve(estimatedSize);
    result.push_back('{');
    bool first = true;

    for (const auto& key : keys) {
        auto it = data.find(key);
        if (it != data.end()) {
            if (!first) {
                result.push_back(',');
            }
            first = false;

            result.push_back('"');
            result += key;
            result += "\":";

            const std::string& value = it->second;

            if (!value.empty() && (value[0] == '{' || value[0] == '[')) {
                result += value;
            } else if (value == "true" || value == "false" || value == "null") {
                result += value;
            } else {
                if (isJsonNumberLiteral(value)) {
                    result += value;
                } else {
                    result += stringToString(value);
                }
            }
        }
    }
    
    result.push_back('}');
    return result;
}

std::string JSONParser::arrayToString() const {
    std::vector<std::string> serializedItems;
    serializedItems.reserve(arrayData.size());

    size_t estimatedSize = 2;  // []
    if (!arrayData.empty()) {
        estimatedSize += arrayData.size() - 1;  // commas
    }
    for (const JSONParser& item : arrayData) {
        serializedItems.push_back(item.toString());
        estimatedSize += serializedItems.back().size();
    }

    std::string result;
    result.reserve(estimatedSize);
    result.push_back('[');
    for (size_t i = 0; i < serializedItems.size(); ++i) {
        if (i > 0) result.push_back(',');
        result += serializedItems[i];
    }
    result.push_back(']');
    return result;
}

std::vector<std::string> JSONParser::getKeys() {
    return keys;
}

bool JSONParser::hasKey(const std::string &key) const {
    return data.find(key) != data.end();
}

}  // namespace geruest