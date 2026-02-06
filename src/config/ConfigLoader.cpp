/**
 * @file ConfigLoader.cpp
 * @date 06.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Configuration loader implementation
 */

#include "ConfigLoader.hpp"

#include <algorithm>
#include <cctype>

namespace geruest {

// Static member initialization
std::unordered_map<std::string, std::string> ConfigLoader::_envFileCache;

bool ConfigLoader::loadEnvFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        // Not an error - .env file is optional
        return false;
    }

    std::string line;
    size_t count = 0;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the '=' separator
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        // Remove quotes if present
        if (value.size() >= 2 && 
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        _envFileCache[key] = value;
        count++;
    }

    if (count > 0) {
        std::cout << "Loaded " << count << " configuration values from " << filename << std::endl;
    }
    
    return true;
}

std::string ConfigLoader::get(const std::string& key, const std::string& defaultValue) {
    // 1. Check .env file cache
    auto it = _envFileCache.find(key);
    if (it != _envFileCache.end()) {
        return it->second;
    }

    // 2. Check environment variables
    const char* envValue = std::getenv(key.c_str());
    if (envValue != nullptr) {
        return std::string(envValue);
    }

    // 3. Return default
    return defaultValue;
}

int ConfigLoader::getInt(const std::string& key, int defaultValue) {
    try {
        std::string value = get(key);
        return value.empty() ? defaultValue : std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

float ConfigLoader::getFloat(const std::string& key, float defaultValue) {
    try {
        std::string value = get(key);
        return value.empty() ? defaultValue : std::stof(value);
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigLoader::getBool(const std::string& key, bool defaultValue) {
    std::string value = toLower(trim(get(key)));
    
    if (value.empty()) {
        return defaultValue;
    }
    
    // Recognized true values
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    
    // Recognized false values
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    
    return defaultValue;
}

size_t ConfigLoader::getSizeT(const std::string& key, size_t defaultValue) {
    try {
        std::string value = get(key);
        return value.empty() ? defaultValue : static_cast<size_t>(std::stoull(value));
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigLoader::has(const std::string& key) {
    // Check .env file cache
    if (_envFileCache.find(key) != _envFileCache.end()) {
        return true;
    }
    
    // Check environment variables
    const char* envValue = std::getenv(key.c_str());
    return envValue != nullptr;
}

void ConfigLoader::clear() {
    _envFileCache.clear();
}

std::string ConfigLoader::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string ConfigLoader::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

}  // namespace geruest
