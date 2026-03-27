#include "JSONParser.hpp"
#include <iostream>
#include <cctype>
#include <stdexcept>
#include "security/Security.hpp"

namespace geruest {

JSONParser::JSONParser(const std::string &input) : basicString(input), jp(0) {
    parseJSON();
}

JSONParser::JSONParser(std::map<std::string, std::string> initialData) : data(std::move(initialData)) {}

bool JSONParser::isWhiteSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string JSONParser::readKey() {
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
    if (jp >= basicString.length() || basicString[jp] != '"') {
        return "";
    }
    jp++;
    std::string key;
    bool escape = false;
    while (jp < basicString.length()) {
        if (escape) {
            key += basicString[jp];
            escape = false;
            jp++;
        } else if (basicString[jp] == '\\') {
            escape = true;
            jp++;
        } else if (basicString[jp] == '"') {
            jp++;
            break;
        } else {
            key += basicString[jp];
            jp++;
        }
    }
    return key;
}

std::string JSONParser::readString() {
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
    if (jp >= basicString.length() || basicString[jp] != '"') {
        return "";
    }
    jp++;
    std::string value;
    bool escape = false;
    while (jp < basicString.length()) {
        if (escape) {
            value += basicString[jp];
            escape = false;
            jp++;
        } else if (basicString[jp] == '\\') {
            escape = true;
            jp++;
        } else if (basicString[jp] == '"') {
            jp++;
            break;
        } else {
            value += basicString[jp];
            jp++;
        }
    }
    return value;
}

std::string JSONParser::readNumber() {
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
    std::string number;
    if (jp < basicString.length() && basicString[jp] == '-') {
        number += basicString[jp];
        jp++;
    }
    while (jp < basicString.length() && 
           (std::isdigit(basicString[jp]) || basicString[jp] == '.')) {
        number += basicString[jp];
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
    
    while (jp < basicString.length()) {
        char c = basicString[jp];
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
    
    return basicString.substr(start, jp - start);
}

std::string JSONParser::readNestedArray() {
    int bracketCount = 0;
    size_t start = jp;
    bool inString = false;
    bool escape = false;

    while (jp < basicString.length()) {
        char c = basicString[jp];
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
    return basicString.substr(start, jp - start);
}

std::string JSONParser::readData() {
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
    if (jp >= basicString.length()) {
        return "";
    }
    char c = basicString[jp];
    if (c == '"') {
        return readString();
    } else if (std::isdigit(c) || c == '-') {
        return readNumber();
    } else if (basicString.substr(jp, 4) == "true") {
        jp += 4;
        return "true";
    } else if (basicString.substr(jp, 5) == "false") {
        jp += 5;
        return "false";
    } else if (basicString.substr(jp, 4) == "null") {
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
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
    
    // Must start with '['
    if (jp >= basicString.length() || basicString[jp] != '[') {
        return;
    }
    jp++;
    
    // Parse array elements
    while (jp < basicString.length()) {
        // Skip whitespace
        while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
            jp++;
        }
        
        // Check for end of array
        if (jp < basicString.length() && basicString[jp] == ']') {
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
        while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
            jp++;
        }
        
        // Check for comma or end of array
        if (jp < basicString.length() && basicString[jp] == ',') {
            jp++;
        } else if (jp < basicString.length() && basicString[jp] == ']') {
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
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
    
    // Check if this is an array or object
    if (jp < basicString.length() && basicString[jp] == '[') {
        // This is an array, parse it
        parseArray();
        return;
    }
    
    // This should be an object
    if (jp >= basicString.length() || basicString[jp] != '{') {
        return;
    }
    jp++;
    
    while (jp < basicString.length()) {
        while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
            jp++;
        }
        if (jp < basicString.length() && basicString[jp] == '}') {
            jp++;
            break;
        }
        
        std::string key = readKey();
        if (key.empty()) break;
        keys.push_back(key);
        
        while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
            jp++;
        }
        if (jp >= basicString.length() || basicString[jp] != ':') {
            break;
        }
        jp++;
        
        std::string value = readData();
        data[key] = value;
        
        while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
            jp++;
        }
        if (jp < basicString.length() && basicString[jp] == ',') {
            jp++;
        } else if (jp < basicString.length() && basicString[jp] == '}') {
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
    data[key] = std::to_string(value);
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
    std::string arrayStr = "[";
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) arrayStr += ",";
        arrayStr += "\"" + Security::escapeJson(value[i]) + "\"";
    }
    arrayStr += "]";
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
    
    std::string result = "{";
    bool first = true;
    
    for (const auto& key : keys) {
        auto it = data.find(key);
        if (it != data.end()) {
            if (!first) {
                result += ",";
            }
            first = false;
            
            result += "\"" + key + "\":";
            
            const std::string& value = it->second;
            
            if (!value.empty() && (value[0] == '{' || value[0] == '[')) {
                result += value;
            } else if (value == "true" || value == "false" || value == "null") {
                result += value;
            } else {
                auto isJsonNumber = [](const std::string& s) -> bool {
                    // RFC 8259 number grammar:
                    // -?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?
                    if (s.empty()) return false;

                    size_t i = 0;
                    const size_t n = s.size();

                    if (s[i] == '-') {
                        ++i;
                        if (i >= n) return false;  // lone '-'
                    }

                    // int part
                    if (s[i] == '0') {
                        ++i;
                        if (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
                            return false;  // leading zero
                        }
                    } else if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                        if (s[i] < '1' || s[i] > '9') return false;
                        ++i;
                        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
                    } else {
                        return false;
                    }

                    // frac part
                    if (i < n && s[i] == '.') {
                        ++i;
                        if (i >= n) return false;  // trailing '.'
                        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
                        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
                    }

                    // exp part
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
                };

                if (isJsonNumber(value)) {
                    result += value;
                } else {
                    result += stringToString(value);
                }
            }
        }
    }
    
    result += "}";
    return result;
}

std::string JSONParser::arrayToString() const {
    std::string result = "[";
    for (size_t i = 0; i < arrayData.size(); ++i) {
        if (i > 0) result += ",";
        result += arrayData[i].toString();
    }
    result += "]";
    return result;
}

std::vector<std::string> JSONParser::getKeys() {
    return keys;
}

bool JSONParser::hasKey(const std::string &key) const {
    return data.find(key) != data.end();
}

}  // namespace geruest