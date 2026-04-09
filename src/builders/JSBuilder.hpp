/**
 * @file JSBuilder.hpp
 * @date on: 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * 
 * When mergeAssets=false: Serves individual JS files as-is.
 * When mergeAssets=true: Serves the per-page merged bundle (same as HTMLBuilder), rebuilt
 * from the page HTML template when necessary so obfuscation always sees one bundle.
 * 
 * Obfuscation support:
 * - Checks if file should be obfuscated (level > 0, not dev mode, not excluded)
 * - Uses disk cache with expiry time (default: 7 days)
 * - Rebuilds obfuscated files when they expire
 */

#ifndef JSBUILDER_HPP
#define JSBUILDER_HPP

#include "ContentBuilder.hpp"
#include <string>
#include <vector>

namespace geruest {

class JSBuilder : public ContentBuilder {
public:

    JSBuilder(const std::string &inputPath, const ServerData& serverData);

private:

    void builJS();
    
    /**
     * Check if a file should be excluded from obfuscation
     * @param filePath Full path to the JS file
     * @return true if file should be excluded
     */
    bool isExcludedFromObfuscation(const std::string& filePath);
    
    /**
     * Check if cached obfuscated file exists and is not expired
     * @param filePath Full path to the JS file
     * @return true if valid cache exists
     */
    bool hasValidObfuscationCache(const std::string& filePath);
    
    /**
     * Apply obfuscation to JS content and save to disk
     * @param content JS content to obfuscate
     * @param filePath Path where to save obfuscated version
     * @return Obfuscated content
     */
    std::string obfuscateAndCache(const std::string& content, const std::string& filePath);
};

}  // namespace geruest

#endif //JSBUILDER_HPP