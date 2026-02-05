/**
 * @file EnvLoader.hpp
 * @date 05.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Simple .env file loader utility
 */

#ifndef GERUEST_ENVLOADER_HPP
#define GERUEST_ENVLOADER_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

class EnvLoader {
   public:
    /**
     * @brief Load environment variables from a .env file
     * @param filename Path to the .env file (default: ".env")
     * @return true if file was loaded successfully, false otherwise
     */
    static bool load(const std::string& filename = ".env") {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open " << filename << std::endl;
            return false;
        }

        std::string line;
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
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            env[key] = value;
        }

        std::cout << "Loaded " << env.size() << " environment variables from " << filename << std::endl;
        return true;
    }

    /**
     * @brief Get an environment variable value
     * @param key The variable name
     * @param defaultValue Default value if not found
     * @return The variable value or default value
     */
    static std::string get(const std::string& key, const std::string& defaultValue = "") {
        auto it = env.find(key);
        return it != env.end() ? it->second : defaultValue;
    }

    /**
     * @brief Get an integer environment variable
     * @param key The variable name
     * @param defaultValue Default value if not found or invalid
     * @return The integer value or default
     */
    static int getInt(const std::string& key, int defaultValue = 0) {
        try {
            std::string value = get(key);
            return value.empty() ? defaultValue : std::stoi(value);
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Check if a variable exists
     * @param key The variable name
     * @return true if exists, false otherwise
     */
    static bool has(const std::string& key) {
        return env.find(key) != env.end();
    }

   private:
    static std::unordered_map<std::string, std::string> env;

    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }
};

// Static member initialization
std::unordered_map<std::string, std::string> EnvLoader::env;

#endif  // GERUEST_ENVLOADER_HPP
