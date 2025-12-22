/**
 * @file JSONParser.hpp
 * @created 24.05.2024
 *
 * @author: Urs Behrmann
 *
 * @brief A simple JSONParser parser that can parse JSONParser strings and return the data as a map of strings and any type.
 */

#ifndef JSONParser_HPP
#define JSONParser_HPP

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <cfloat>
#include <climits>
#include <utility>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>

namespace geruest {

class JSONParser {

private:
    // Map to store the JSONParser data
    std::map<std::string, std::string> data;
    std::vector<JSONParser> arrayData;

    std::vector<std::string> keys;

    std::string basicString;
    size_t jp = 0;

    /**
     * Read the key from the JSONParser string
     * @return The key
     */
    std::string readKey();

    /**
     * Read the string data from the JSONParser string
     * @return The string data
     */
    std::string readString();

    /**
     * Read the number data from the JSONParser string
     * @return The number data as string
     */
    std::string readNumber();

    std::map<std::string, std::string> readObject();

    std::vector<std::string> readArray();

    /**
     * Read a nested object as a JSON string
     * @return The nested object as a JSON string
     */
    std::string readNestedObject();

    /**
     * Read a nested array as a JSON string
     * @return The nested array as a JSON string
     */
    std::string readNestedArray();

    /**
     * Parse array string into individual elements
     * @param arrayStr The array string to parse (e.g., "[1,2,3]")
     * @return Vector of string elements
     */
    std::vector<std::string> parseArrayString(const std::string& arrayStr);

    /**
     * Extract string value by removing quotes
     * @param str The string to extract from
     * @return The extracted string without quotes
     */
    std::string extractString(const std::string& str);

    /**
     * Read the data from the JSONParser string
     * @return The data as string
     */
    std::string readData();

    /**
     * Check if a character is a whitespace character
     * @param c Character to check
     * @return True if the character is a whitespace character, false otherwise
     */
    static bool isWhiteSpace(char c);

    void parseArray();

    void parseJSON();

    std::string stringToString(const std::string &val) const;

public:

    JSONParser() = default;

    explicit JSONParser(const std::string &input);

    explicit JSONParser(std::map<std::string, std::string> initialData);

    ~JSONParser() = default;

    // Getter for the data

    std::string getString(const std::string &key);

    int getInt(const std::string &key);

    short getShort(const std::string &key);

    bool getBool(const std::string &key);

    float getFloat(const std::string &key);

    double getDouble(const std::string &key);

    long getLong(const std::string &key);

    long long getLongLong(const std::string &key);

    long double getLongDouble(const std::string &key);

    JSONParser getObject(const std::string &key);

    std::vector<std::string> getStringArray(const std::string &key);

    std::vector<short> getShortArray(const std::string &key);

    std::vector<int> getIntArray(const std::string &key);

    std::vector<long> getLongArray(const std::string &key);

    std::vector<long long> getLongLongArray(const std::string &key);

    std::vector<bool> getBoolArray(const std::string &key);

    std::vector<float> getFloatArray(const std::string &key);

    std::vector<double> getDoubleArray(const std::string &key);

    std::vector<long double> getLongDoubleArray(const std::string &key);

    std::vector<JSONParser> getArrayOfJSON(const std::string &key);

    std::vector<JSONParser> getJSONArray();

    // Setter for the data

    void setString(const std::string &key, const std::string &value);

    void setInt(const std::string &key, int value);

    void setShort(const std::string &key, short value);

    void setBool(const std::string &key, bool value);

    void setFloat(const std::string &key, float value);

    void setDouble(const std::string &key, double value);

    void setLong(const std::string &key, long value);

    void setLongLong(const std::string &key, long long value);

    void setLongDouble(const std::string &key, long double value);

    void setJSON(const std::string &key, const JSONParser &value);

    void setStringArray(const std::string &key, const std::vector<std::string> &value);

    void setShortArray(const std::string &key, const std::vector<short> &value);

    void setIntArray(const std::string &key, const std::vector<int> &value);

    void setLongArray(const std::string &key, const std::vector<long> &value);

    void setLongLongArray(const std::string &key, const std::vector<long long> &value);

    void setBoolArray(const std::string &key, const std::vector<bool> &value);

    void setFloatArray(const std::string &key, const std::vector<float> &value);

    void setDoubleArray(const std::string &key, const std::vector<double> &value);

    void setLongDoubleArray(const std::string &key, const std::vector<long double> &value);

    void setArrayOfJSON(const std::string &key, const std::vector<JSONParser> &value);

    void setJSONArray(const std::vector<JSONParser> &value);

    // Add a key to the JSONParser

    void addArrayOfJSON(const std::string &key, const std::vector<JSONParser> &value);

    void addJSONToArray(const JSONParser &value);

    // Remove a key from the JSONParser
    void removeKey(const std::string &key);

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] std::string arrayToString() const;

    // Get the keys of the JSONParser
    std::vector<std::string> getKeys();
    
    // Check if a key exists
    bool hasKey(const std::string &key) const;
};

/**
 * @brief Load JSON from file (safe version with automatic memory management)
 * @param filePath Path to the JSON file
 * @return unique_ptr to JSONParser, nullptr if file couldn't be opened
 * @note Memory is automatically freed when unique_ptr goes out of scope
 */
inline std::unique_ptr<JSONParser> getJSONFromFileSafe(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return std::make_unique<JSONParser>(content);
}

/**
 * @brief Load JSON from file (unsafe version with manual memory management)
 * @param filePath Path to the JSON file
 * @return Raw pointer to JSONParser, nullptr if file couldn't be opened
 * @warning Caller is responsible for calling delete on the returned pointer
 * @deprecated Prefer getJSONFromFile() which returns unique_ptr for automatic cleanup
 */
inline JSONParser* getJSONFromFile(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return new JSONParser(content);
}

inline bool saveJSONToFile(const JSONParser &json, const std::string &filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << json.toString();
    file.close();

    return true;
}

// Save Array JSON to file
inline bool saveArrayJSONToFile(const JSONParser &json, const std::string &filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << json.arrayToString();
    file.close();

    return true;
}

}  // namespace geruest

#endif //JSONParser_HPP