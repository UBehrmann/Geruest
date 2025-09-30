#include "JSONParser.hpp"
#include <iostream>
#include <cctype>
#include <stdexcept>

namespace geruest {

JSONParser::JSONParser(const std::string &input) : basicString(input), jp(0) {
    parseJSON();
}

JSONParser::JSONParser(std::map<std::string, std::string> data) : data(std::move(data)) {}

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
    while (jp < basicString.length() && basicString[jp] != '"') {
        key += basicString[jp];
        jp++;
    }
    if (jp < basicString.length()) {
        jp++;
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
    while (jp < basicString.length() && basicString[jp] != '"') {
        value += basicString[jp];
        jp++;
    }
    if (jp < basicString.length()) {
        jp++;
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
    
    while (jp < basicString.length()) {
        if (basicString[jp] == '{') {
            braceCount++;
        } else if (basicString[jp] == '}') {
            braceCount--;
            if (braceCount == 0) {
                jp++; // Move past the closing brace
                break;
            }
        }
        jp++;
    }
    
    return basicString.substr(start, jp - start);
}

std::string JSONParser::readNestedArray() {
    int bracketCount = 0;
    size_t start = jp;
    
    while (jp < basicString.length()) {
        if (basicString[jp] == '[') {
            bracketCount++;
        } else if (basicString[jp] == ']') {
            bracketCount--;
            if (bracketCount == 0) {
                jp++; // Move past the closing bracket
                break;
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
}

void JSONParser::parseJSON() {
    data.clear();
    keys.clear();
    jp = 0;
    
    while (jp < basicString.length() && isWhiteSpace(basicString[jp])) {
        jp++;
    }
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
    return "\"" + val + "\"";
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

std::vector<std::string> JSONParser::getStringArray(const std::string &key) {
    std::vector<std::string> result;
    return result;
}

std::vector<short> JSONParser::getShortArray(const std::string &key) {
    std::vector<short> result;
    return result;
}

std::vector<int> JSONParser::getIntArray(const std::string &key) {
    std::vector<int> result;
    return result;
}

std::vector<long> JSONParser::getLongArray(const std::string &key) {
    std::vector<long> result;
    return result;
}

std::vector<long long> JSONParser::getLongLongArray(const std::string &key) {
    std::vector<long long> result;
    return result;
}

std::vector<bool> JSONParser::getBoolArray(const std::string &key) {
    std::vector<bool> result;
    return result;
}

std::vector<float> JSONParser::getFloatArray(const std::string &key) {
    std::vector<float> result;
    return result;
}

std::vector<double> JSONParser::getDoubleArray(const std::string &key) {
    std::vector<double> result;
    return result;
}

std::vector<long double> JSONParser::getLongDoubleArray(const std::string &key) {
    std::vector<long double> result;
    return result;
}

std::vector<JSONParser> JSONParser::getArrayOfJSON(const std::string &key) {
    std::vector<JSONParser> result;
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
        arrayStr += "\"" + value[i] + "\"";
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
                bool isNumber = true;
                if (!value.empty()) {
                    for (size_t i = 0; i < value.length(); ++i) {
                        char c = value[i];
                        if (!std::isdigit(c) && c != '.' && c != '-') {
                            isNumber = false;
                            break;
                        }
                    }
                }
                
                if (isNumber && !value.empty()) {
                    result += value;
                } else {
                    result += "\"" + value + "\"";
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