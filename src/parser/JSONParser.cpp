/**
 * @file JSONParser.cpp
 * @date 29.07.2024
 *
 * @author Urs Behrmann
 *
 * @brief JSONParser class implementation
 */

#include "JSONParser.hpp"

#include <string>
#include <iostream>
#include <map>
#include <any>
#include <cfloat>
#include <climits>
#include <utility>
#include <vector>

JSONParser::JSONParser(const std::string &input) {

    basicString = input;

    if (input.empty()) {
        throw(std::invalid_argument("Empty input"));
    }

    if (input[jp] == '[') {
        parseArray();
    } else {
        parseJSON();
    }


}

void JSONParser::parseArray() {

    if (basicString[jp] != '[') {
        throw std::invalid_argument("Expected '[' at the beginning of array");
    }
    jp++; // Move past '['

    size_t sp;
    size_t ep;

    while (jp < basicString.size() && basicString[jp] != ']') {

        if (basicString[jp] == '{') {
            sp = jp;
        }
        if(basicString[jp] == '}') {
            ep = jp;
            arrayData.emplace_back(basicString.substr(sp, ep - sp + 1));
        }
        jp++;
    }

    if (basicString[jp] == ']') {
        jp++; // Move past ']'
    } else {
        throw std::invalid_argument("Expected ']' at the end of array");
    }

}

void JSONParser::parseJSON() {

    std::string key;

    data.clear();

    for (; jp < basicString.size();) {

        if (basicString[jp] == '}') {
            return;
        } else if (basicString[jp] == '{' || basicString[jp] == ',') {

            key = readKey();

        } else if (basicString[jp] == ':') {
            std::any value = readData();
            data[key] = value;
            keys.push_back(key);
        } else {
            jp++;
        }
    }

    throw (std::invalid_argument("Invalid JSONParser"));

}

JSONParser::JSONParser(std::map<std::string, std::any> data) : data(std::move(data)) {
    for (const auto &it : this->data) {
        keys.push_back(it.first);
    }
}

std::string JSONParser::toString() const {
    std::string result = "{\n";
    for (const std::string &key: keys) {
        result += "\t\"" + key + "\" : ";
        auto val = data.at(key);
        if (val.type() == typeid(std::string)) {
            result += "\"" + std::any_cast<std::string>(val) + "\",";
        } else if (val.type() == typeid(int)) {
            result += std::to_string(std::any_cast<int>(val)) + ",";
        } else if (val.type() == typeid(short)) {
            result += std::to_string(std::any_cast<short>(val)) + ",";
        } else if (val.type() == typeid(bool)) {
            result += std::to_string(std::any_cast<bool>(val)) + ",";
        } else if (val.type() == typeid(float)) {
            result += std::to_string(std::any_cast<float>(val)) + ",";
        } else if (val.type() == typeid(double)) {
            result += std::to_string(std::any_cast<double>(val)) + ",";
        } else if (val.type() == typeid(long)) {
            result += std::to_string(std::any_cast<long>(val)) + ",";
        } else if (val.type() == typeid(long long)) {
            result += std::to_string(std::any_cast<long long>(val)) + ",";
        } else if (val.type() == typeid(long double)) {
            result += std::to_string(std::any_cast<long double>(val)) + ",";
        } else if (val.type() == typeid(std::map<std::string, std::any>)) {

            JSONParser json(std::any_cast<std::map<std::string, std::any>>(val));

            result += json.toString() + ",";
        } else if (val.type() == typeid(std::vector<std::any>)) {
            result += "[";
            for (auto const &v: std::any_cast<std::vector<std::any>>(val)) {
                if (v.type() == typeid(std::string)) {
                    result += "\"" + std::any_cast<std::string>(v) + "\",";
                } else if (v.type() == typeid(int)) {
                    result += std::to_string(std::any_cast<int>(v)) + ",";
                } else if (v.type() == typeid(short)) {
                    result += std::to_string(std::any_cast<short>(v)) + ",";
                } else if (v.type() == typeid(bool)) {
                    result += std::to_string(std::any_cast<bool>(v)) + ",";
                } else if (v.type() == typeid(float)) {
                    result += std::to_string(std::any_cast<float>(v)) + ",";
                } else if (v.type() == typeid(double)) {
                    result += std::to_string(std::any_cast<double>(v)) + ",";
                } else if (v.type() == typeid(long)) {
                    result += std::to_string(std::any_cast<long>(v)) + ",";
                } else if (v.type() == typeid(long long)) {
                    result += std::to_string(std::any_cast<long long>(v)) + ",";
                } else if (v.type() == typeid(long double)) {
                    result += std::to_string(std::any_cast<long double>(v)) + ",";
                } else if (v.type() == typeid(std::map<std::string, std::any>)) {
                    result += JSONParser(std::any_cast<std::map<std::string, std::any>>(v)).toString() + ",";
                }
            }

            if(!std::any_cast<std::vector<std::any>>(val).empty()) result.pop_back();

            result += "],";

        } else if(val.type() == typeid(std::vector<JSONParser>)) {
            result += "[";
            for (const auto &v : std::any_cast<std::vector<JSONParser>>(val)) {
                result += v.toString() + ",";
            }

            if(!std::any_cast<std::vector<JSONParser>>(val).empty()) result.pop_back();
            result += "],";
        }
        result += "\n";
    }
    return result.substr(0, result.size() - 2) + "\n}";
}

std::vector<std::string> JSONParser::getKeys() {
    return keys;
}

std::string JSONParser::readKey() {

    while (jp < basicString.size()) {

        if (basicString[jp] == '}') {
            return "";
        }

        if (basicString[jp] == '"') {

            std::string key;
            jp++;

            while (basicString[jp] != '"') {
                key += basicString[jp];
                jp++;
            }

            jp++;

            return key;
        }

        jp++;
    }

    return "";
}

std::string JSONParser::readString() {
    std::string stringData;

    jp++;

    while (basicString[jp] != '"') {
        stringData += basicString[jp];
        jp++;
    }

    return stringData;
}

std::any JSONParser::readNumber() {

    bool isNegative = false;
    long long number = 0;
    long double decimal = 0;

    // Check if the number is negative
    if (basicString[jp] == '-') {
        isNegative = true;
        jp++;
    }

    // Read the integer part of the number
    while (isdigit(basicString[jp])) {
        number = number * 10 + (basicString[jp] - '0');
        jp++;
    }

    // Check if the number has a decimal part
    if (basicString[jp] == '.') {
        jp++;
        decimal = number;
        long double decimalPlace = 0.1;

        while (isdigit(basicString[jp])) {
            decimal += (basicString[jp] - '0') * decimalPlace;
            decimalPlace /= 10;
            jp++;
        }

        if (isNegative) {
            decimal = -decimal;
        }

        // Return in the smallest possible float type
        if (decimal <= FLT_MAX && decimal >= -FLT_MAX) {
            return static_cast<float>(decimal);
        } else if (decimal <= DBL_MAX && decimal >= -DBL_MAX) {
            return static_cast<double>(decimal);
        } else {
            return decimal;
        }

    } else {
        if (isNegative) {
            number = -number;
        }

        if (number <= SHRT_MAX && number >= SHRT_MIN) {
            return static_cast<short>(number);
        } else if (number <= INT_MAX && number >= INT_MIN) {
            return static_cast<int>(number);
        } else if (number <= LONG_MAX) {
            return static_cast<long>(number);
        } else {
            return number; // returns as int64_t if it's outside the range of long
        }
    }
}

std::map<std::string, std::any> JSONParser::readObject() {
    std::string key;
    std::map<std::string, std::any> objectData;

    while (jp < basicString.size()) {

        if (basicString[jp] == '{' || basicString[jp] == ',') {

            key = readKey();
        } else if (basicString[jp] == ':') {

            std::any value = readData();
            objectData[key] = value;
        } else if (basicString[jp] == '}') {

            jp++;

            return objectData;
        } else {
            jp++;
        }
    }

    return objectData;
}

std::vector<std::any> JSONParser::readArray() {

    std::string key;
    std::vector<std::any> arrayData;

    jp++;

    while (basicString[jp] != ']') {

        arrayData.push_back(readData());

        while (basicString[jp] != ',' && basicString[jp] != ']') {
            jp++;
        }
    }

    jp++;

    return arrayData;
}

std::any JSONParser::readData() {

    while (jp < basicString.size()) {

        // Move to the next character if the current character is a whitespace character
        if (isWhiteSpace(basicString[jp])) {
            jp++;
            continue;
        }

        // Check if the data is a string
        if (basicString[jp] == '"') {
            return readString();
        }

        // Check if the data is a number
        if (isdigit(basicString[jp]) || basicString[jp] == '-') {
            return readNumber();
        }

        // Check if the data is a boolean
        if (basicString[jp] == 't' || basicString[jp] == 'f') {
            if (basicString[jp] == 't') {
                return true;
            } else {
                return false;
            }
        }

        // Check if the data is a null value
        if (basicString[jp] == 'n') {
            return nullptr;
        }

        // Check if the data is an object
        if (basicString[jp] == '{') {
            return readObject();
        }

        // Check if the data is an array
        if (basicString[jp] == '[') {
            return readArray();
        }

        jp++;
    }

    return NULL;
}

bool JSONParser::isWhiteSpace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

std::string JSONParser::getString(const std::string &key) {

    // Check if the key exists
    if (data.find(key) == data.end() || data[key].type() != typeid(std::string)) {

        // Return an std::string if the data is a number
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                return std::to_string(std::any_cast<int>(data[key]));
            } else if (data[key].type() == typeid(long)) {
                return std::to_string(std::any_cast<long>(data[key]));
            } else if (data[key].type() == typeid(long long)) {
                return std::to_string(std::any_cast<long long>(data[key]));
            } else if (data[key].type() == typeid(float)) {
                return std::to_string(std::any_cast<float>(data[key]));
            } else if (data[key].type() == typeid(double)) {
                return std::to_string(std::any_cast<double>(data[key]));
            } else if (data[key].type() == typeid(long double)) {
                return std::to_string(std::any_cast<long double>(data[key]));
            } else if (data[key].type() == typeid(short)) {
                return std::to_string(std::any_cast<short>(data[key]));
            } else if (data[key].type() == typeid(bool)) {
                return std::to_string(std::any_cast<bool>(data[key]));
            }
        }

        return {""};
    }

    return std::any_cast<std::string>(data[key]);
}

int JSONParser::getInt(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(int)) {

        // Return an int if the number is within the range of an int
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                if (number <= INT_MAX && number >= INT_MIN) {
                    return static_cast<int>(number);
                }
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                if (number <= INT_MAX && number >= INT_MIN) {
                    return static_cast<int>(number);
                }
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                if (number <= INT_MAX && number >= INT_MIN) {
                    return static_cast<int>(number);
                }
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                if (number <= INT_MAX && number >= INT_MIN) {
                    return static_cast<int>(number);
                }
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                if (number <= INT_MAX && number >= INT_MIN) {
                    return static_cast<int>(number);
                }
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                return static_cast<int>(number);
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                return static_cast<int>(number);
            } else if(data[key].type() == typeid(std::string)) {
                try {
                    return std::stoi(std::any_cast<std::string>(data[key]));
                } catch (std::invalid_argument &e) {
                    return 0;
                }
            }
        }

        return 0;
    }

    return std::any_cast<int>(data[key]);
}

short JSONParser::getShort(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(short)) {

        // Return a short if the number is within the range of a short
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                if (number <= SHRT_MAX && number >= SHRT_MIN) {
                    return static_cast<short>(number);
                }
            } else if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                if (number <= SHRT_MAX && number >= SHRT_MIN) {
                    return static_cast<short>(number);
                }
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                if (number <= SHRT_MAX && number >= SHRT_MIN) {
                    return static_cast<short>(number);
                }
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                if (number <= SHRT_MAX && number >= SHRT_MIN) {
                    return static_cast<short>(number);
                }
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                if (number <= SHRT_MAX && number >= SHRT_MIN) {
                    return static_cast<short>(number);
                }
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                if (number <= SHRT_MAX && number >= SHRT_MIN) {
                    return static_cast<short>(number);
                }
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                return static_cast<short>(number);
            }
        }

        return 0;
    }

    return std::any_cast<short>(data[key]);
}

bool JSONParser::getBool(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(bool)) {

        // Return a bool if the number is within the range of a bool
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            } else if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                if (number <= 1 && number >= 0) {
                    return static_cast<bool>(number);
                }
            }
        }

        return false;
    }

    return std::any_cast<bool>(data[key]);
}

float JSONParser::getFloat(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(float)) {

        // Return a float if the number is within the range of a float
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            } else if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                if (number <= FLT_MAX && number >= -FLT_MAX) {
                    return static_cast<float>(number);
                }
            }
        }

        return 0.0f;
    }

    return std::any_cast<float>(data[key]);
}

double JSONParser::getDouble(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(double)) {

        // Return a double if the number is within the range of a double
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            } else if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                if (number <= DBL_MAX && number >= -DBL_MAX) {
                    return static_cast<double>(number);
                }
            }
        }

        return 0.0;
    }

    return std::any_cast<double>(data[key]);
}

long JSONParser::getLong(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(long)) {

        // Return a long if the number is within the range of a long
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                if (number <= LONG_MAX) {
                    return static_cast<long>(number);
                }
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                if (number <= LONG_MAX) {
                    return static_cast<long>(number);
                }
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                if (number <= LONG_MAX) {
                    return static_cast<long>(number);
                }
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                if (number <= LONG_MAX) {
                    return static_cast<long>(number);
                }
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                if (number <= LONG_MAX) {
                    return static_cast<long>(number);
                }
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                return static_cast<long>(number);
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                return static_cast<long>(number);
            }
        }

        return 0;
    }

    return std::any_cast<long>(data[key]);
}

long long JSONParser::getLongLong(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(long long)) {

        // Return a long long if the number is within the range of a long long
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                return static_cast<long long>(number);
            } else if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                return static_cast<long long>(number);
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                return static_cast<long long>(number);
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                return static_cast<long long>(number);
            } else if (data[key].type() == typeid(long double)) {
                auto number = std::any_cast<long double>(data[key]);
                return static_cast<long long>(number);
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                return static_cast<long long>(number);
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                return static_cast<long long>(number);
            }
        }

        return 0;
    }

    return std::any_cast<long long>(data[key]);
}

long double JSONParser::getLongDouble(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(long double)) {

        // Return a long double if the number is within the range of a long double
        if (data.find(key) != data.end()) {
            if (data[key].type() == typeid(int)) {
                auto number = std::any_cast<int>(data[key]);
                return static_cast<long double>(number);
            } else if (data[key].type() == typeid(long)) {
                auto number = std::any_cast<long>(data[key]);
                return static_cast<long double>(number);
            } else if (data[key].type() == typeid(long long)) {
                auto number = std::any_cast<long long>(data[key]);
                return static_cast<long double>(number);
            } else if (data[key].type() == typeid(float)) {
                auto number = std::any_cast<float>(data[key]);
                return static_cast<long double>(number);
            } else if (data[key].type() == typeid(double)) {
                auto number = std::any_cast<double>(data[key]);
                return static_cast<long double>(number);
            } else if (data[key].type() == typeid(short)) {
                auto number = std::any_cast<short>(data[key]);
                return static_cast<long double>(number);
            } else if (data[key].type() == typeid(bool)) {
                auto number = std::any_cast<bool>(data[key]);
                return static_cast<long double>(number);
            }
        }

        return 0.0;
    }

    return std::any_cast<long double>(data[key]);
}

JSONParser JSONParser::getObject(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::map<std::string, std::any>)) {
        return JSONParser("");
    }

    return JSONParser(std::any_cast<std::map<std::string, std::any>>(data[key]));
}

std::vector<std::string> JSONParser::getStringArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<std::string> stringArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(std::string)) {
            stringArray.push_back(std::any_cast<std::string>(it));
        }
    }

    return stringArray;
}

std::vector<short> JSONParser::getShortArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<short> shortArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(short)) {
            shortArray.push_back(std::any_cast<short>(it));
        } else {
            if (it.type() == typeid(int)) {
                shortArray.push_back(static_cast<short>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long)) {
                shortArray.push_back(static_cast<short>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(long long)) {
                shortArray.push_back(static_cast<short>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(float)) {
                shortArray.push_back(static_cast<short>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(double)) {
                shortArray.push_back(static_cast<short>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(long double)) {
                shortArray.push_back(static_cast<short>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(bool)) {
                shortArray.push_back(static_cast<short>(std::any_cast<bool>(it)));
            }
        }
    }

    return shortArray;
}

std::vector<int> JSONParser::getIntArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<int> intArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(int)) {
            intArray.push_back(std::any_cast<int>(it));
        } else {
            if (it.type() == typeid(long)) {
                intArray.push_back(static_cast<int>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(long long)) {
                intArray.push_back(static_cast<int>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(float)) {
                intArray.push_back(static_cast<int>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(double)) {
                intArray.push_back(static_cast<int>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(long double)) {
                intArray.push_back(static_cast<int>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(short)) {
                intArray.push_back(static_cast<int>(std::any_cast<short>(it)));
            } else if (it.type() == typeid(bool)) {
                intArray.push_back(static_cast<int>(std::any_cast<bool>(it)));
            }
        }
    }

    return intArray;
}

std::vector<long> JSONParser::getLongArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<long> longArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(long)) {
            longArray.push_back(std::any_cast<long>(it));
        } else {
            if (it.type() == typeid(int)) {
                longArray.push_back(static_cast<long>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long long)) {
                longArray.push_back(static_cast<long>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(float)) {
                longArray.push_back(static_cast<long>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(double)) {
                longArray.push_back(static_cast<long>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(long double)) {
                longArray.push_back(static_cast<long>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(short)) {
                longArray.push_back(static_cast<long>(std::any_cast<short>(it)));
            } else if (it.type() == typeid(bool)) {
                longArray.push_back(static_cast<long>(std::any_cast<bool>(it)));
            }
        }
    }

    return longArray;
}

std::vector<long long> JSONParser::getLongLongArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<long long> longLongArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(long long)) {
            longLongArray.push_back(std::any_cast<long long>(it));
        } else {
            if (it.type() == typeid(int)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(float)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(double)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(long double)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(short)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<short>(it)));
            } else if (it.type() == typeid(bool)) {
                longLongArray.push_back(static_cast<long long>(std::any_cast<bool>(it)));
            }
        }
    }

    return longLongArray;
}

std::vector<bool> JSONParser::getBoolArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<bool> boolArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(bool)) {
            boolArray.push_back(std::any_cast<bool>(it));
        } else {
            if (it.type() == typeid(int)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(long long)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(float)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(double)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(long double)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(short)) {
                boolArray.push_back(static_cast<bool>(std::any_cast<short>(it)));
            }
        }
    }

    return boolArray;
}

std::vector<float> JSONParser::getFloatArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<float> floatArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(float)) {
            floatArray.push_back(std::any_cast<float>(it));
        } else {
            if (it.type() == typeid(int)) {
                floatArray.push_back(static_cast<float>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long)) {
                floatArray.push_back(static_cast<float>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(long long)) {
                floatArray.push_back(static_cast<float>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(double)) {
                floatArray.push_back(static_cast<float>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(long double)) {
                floatArray.push_back(static_cast<float>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(short)) {
                floatArray.push_back(static_cast<float>(std::any_cast<short>(it)));
            } else if (it.type() == typeid(bool)) {
                floatArray.push_back(static_cast<float>(std::any_cast<bool>(it)));
            }
        }
    }

    return floatArray;
}

std::vector<double> JSONParser::getDoubleArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<double> doubleArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(double)) {
            doubleArray.push_back(std::any_cast<double>(it));
        } else {
            if (it.type() == typeid(int)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(long long)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(float)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(long double)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<long double>(it)));
            } else if (it.type() == typeid(short)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<short>(it)));
            } else if (it.type() == typeid(bool)) {
                doubleArray.push_back(static_cast<double>(std::any_cast<bool>(it)));
            }
        }
    }

    return doubleArray;
}

std::vector<long double> JSONParser::getLongDoubleArray(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<long double> longDoubleArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(long double)) {
            longDoubleArray.push_back(std::any_cast<long double>(it));
        } else {
            if (it.type() == typeid(int)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<int>(it)));
            } else if (it.type() == typeid(long)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<long>(it)));
            } else if (it.type() == typeid(long long)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<long long>(it)));
            } else if (it.type() == typeid(float)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<float>(it)));
            } else if (it.type() == typeid(double)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<double>(it)));
            } else if (it.type() == typeid(short)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<short>(it)));
            } else if (it.type() == typeid(bool)) {
                longDoubleArray.push_back(static_cast<long double>(std::any_cast<bool>(it)));
            }
        }
    }

    return longDoubleArray;
}

std::vector<JSONParser> JSONParser::getArrayOfJSON(const std::string &key) {

    if (data.find(key) == data.end() || data[key].type() != typeid(std::vector<std::any>)) {

        // Return an empty vector if the data is not an array
        return {};
    }

    auto anyArray = std::any_cast<std::vector<std::any>>(data[key]);
    std::vector<JSONParser> objectArray;

    for (const auto &it: anyArray) {
        if (it.type() == typeid(std::map<std::string, std::any>)) {
            objectArray.emplace_back(std::any_cast<std::map<std::string, std::any>>(it));
        }
    }

    return objectArray;
}

void JSONParser::setString(const std::string &key, const std::string &value) {

    data[key] = value;
}

void JSONParser::setInt(const std::string &key, int value) {

    data[key] = value;
}

void JSONParser::setShort(const std::string &key, short value) {

    data[key] = value;
}

void JSONParser::setBool(const std::string &key, bool value) {

    data[key] = value;
}

void JSONParser::setFloat(const std::string &key, float value) {

    data[key] = value;
}

void JSONParser::setDouble(const std::string &key, double value) {

    data[key] = value;
}

void JSONParser::setLong(const std::string &key, long value) {

    data[key] = value;
}

void JSONParser::setLongLong(const std::string &key, long long int value) {

    data[key] = value;
}

void JSONParser::setLongDouble(const std::string &key, long double value) {

    data[key] = value;
}

void JSONParser::setJSON(const std::string &key, const JSONParser &value) {

    data[key] = value.data;
}

void JSONParser::setStringArray(const std::string &key, const std::vector<std::string> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setShortArray(const std::string &key, const std::vector<short> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setIntArray(const std::string &key, const std::vector<int> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setLongArray(const std::string &key, const std::vector<long> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setLongLongArray(const std::string &key, const std::vector<long long int> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setBoolArray(const std::string &key, const std::vector<bool> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setFloatArray(const std::string &key, const std::vector<float> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setDoubleArray(const std::string &key, const std::vector<double> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setLongDoubleArray(const std::string &key, const std::vector<long double> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it);
    }

    data[key] = anyArray;
}

void JSONParser::setArrayOfJSON(const std::string &key, const std::vector<JSONParser> &value) {

    std::vector<std::any> anyArray;

    anyArray.reserve(value.size());

    for (const auto &it: value) {
        anyArray.emplace_back(it.data);
    }

    data[key] = anyArray;
}

void JSONParser::setJSONArray(const std::vector<JSONParser>& value) {

    arrayData = value;
}

void JSONParser::addJSONToArray(const JSONParser& value) {

    arrayData.push_back(value);
}

void JSONParser::addArrayOfJSON(const std::string& key, const std::vector<JSONParser>& value) {
    data[key] = value;
    keys.push_back(key);
}

void JSONParser::removeKey(const std::string &key) {

    data.erase(key);
}

std::string JSONParser::arrayToString() const {
    std::string result = "[\n";
    for (const auto &it: arrayData) {
        result += it.toString() + ",\n";
    }

    if (!arrayData.empty()) {
        result = result.substr(0, result.size() - 2);
    }
    result += "\n]";
    return result;
}

std::vector<JSONParser> JSONParser::getJSONArray() {
    return arrayData;
}
