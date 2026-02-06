/**
 * @file ConfigLoader.hpp
 * @date 06.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Configuration loader with hierarchy: Code > .env > environment variables
 */

#ifndef GERUEST_CONFIGLOADER_HPP
#define GERUEST_CONFIGLOADER_HPP

#include <cstdlib>     // For getenv
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace geruest {

/**
 * @brief Configuration loader supporting .env files and environment variables
 * 
 * Hierarchy for configuration values:
 * 1. Code (explicit setter calls) - highest priority
 * 2. .env file values - middle priority
 * 3. Environment variables (getenv) - lowest priority
 */
class ConfigLoader {
   public:
    /**
     * @brief Load configuration from a .env file
     * @param filename Path to the .env file (default: ".env")
     * @return true if file was loaded successfully, false otherwise
     * 
     * .env file format:
     * KEY=value
     * KEY="value with spaces"
     * # Comments start with #
     */
    static bool loadEnvFile(const std::string& filename = ".env");

    /**
     * @brief Get a configuration value with fallback hierarchy
     * @param key The configuration key
     * @param defaultValue Default value if not found anywhere
     * @return Value from .env file, or environment variable, or default
     * 
     * Search order:
     * 1. Check .env file cache
     * 2. Check environment variables (getenv)
     * 3. Return default value
     */
    static std::string get(const std::string& key, const std::string& defaultValue = "");

    /**
     * @brief Get an integer configuration value
     * @param key The configuration key
     * @param defaultValue Default value if not found or invalid
     * @return Integer value or default
     */
    static int getInt(const std::string& key, int defaultValue = 0);

    /**
     * @brief Get a float configuration value
     * @param key The configuration key
     * @param defaultValue Default value if not found or invalid
     * @return Float value or default
     */
    static float getFloat(const std::string& key, float defaultValue = 0.0f);

    /**
     * @brief Get a boolean configuration value
     * @param key The configuration key
     * @param defaultValue Default value if not found or invalid
     * @return Boolean value or default
     * 
     * Recognized true values: "true", "1", "yes", "on" (case-insensitive)
     * Recognized false values: "false", "0", "no", "off" (case-insensitive)
     */
    static bool getBool(const std::string& key, bool defaultValue = false);

    /**
     * @brief Get a size_t configuration value
     * @param key The configuration key
     * @param defaultValue Default value if not found or invalid
     * @return size_t value or default
     */
    static size_t getSizeT(const std::string& key, size_t defaultValue = 0);

    /**
     * @brief Check if a configuration key exists
     * @param key The configuration key
     * @return true if exists in .env file or environment variables
     */
    static bool has(const std::string& key);

    /**
     * @brief Clear all loaded .env file values
     */
    static void clear();

   private:
    static std::unordered_map<std::string, std::string> _envFileCache;

    /**
     * @brief Trim whitespace from string
     */
    static std::string trim(const std::string& str);

    /**
     * @brief Convert string to lowercase
     */
    static std::string toLower(const std::string& str);
};

}  // namespace geruest

#endif  // GERUEST_CONFIGLOADER_HPP
