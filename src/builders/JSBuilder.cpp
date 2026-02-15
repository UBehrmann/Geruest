/**
 * @file JSBuilder.cpp
 * @date on: 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * 
 * When mergeAssets=false: Serves individual JS files as-is.
 * When mergeAssets=true: Serves pre-generated merged JS files created by HTMLBuilder.
 * 
 * Obfuscation flow:
 * 1. Check if file is excluded -> serve original
 * 2. Check if dev mode or level=0 -> serve original (possibly merged)
 * 3. Check if cached obfuscated file exists and is valid -> serve cached
 * 4. Otherwise: obfuscate, cache to disk, and serve
 */

#include "JSBuilder.hpp"
#include "HTMLBuilder.hpp"
#include "JSObfuscator.hpp"
#include "../FileManagement/FileManagement.hpp"
#include <filesystem>
#include <chrono>

namespace geruest {

JSBuilder::JSBuilder(const std::string &inputPath, const ServerData& serverData) 
    : ContentBuilder(inputPath, serverData) {
    builJS();
}

void JSBuilder::builJS() {
    // Extract filename from path for exclusion checking
    std::filesystem::path filePath(path);
    std::string filename = filePath.filename().string();
    
    // Check if file is excluded from obfuscation and merging
    if (isExcludedFromObfuscation(filename)) {
        // File is excluded - serve as-is (no obfuscation, no merging)
        // Just handle comment removal if enabled
        if (_serverData.getRemoveComments() && !builtFile.empty()) {
            builtFile = removeCommentsFromString(builtFile, FILETYPE_JS);
        }
        return;
    }
    
    // In dev mode with merging, check if content is in cache first
    if (_serverData.isDevMode() && _serverData.getMergeAssets()) {
        // Extract relative path from full path (remove root)
        std::string relativePath = path;
        size_t rootPos = relativePath.find("/assets/");
        if (rootPos != std::string::npos) {
            relativePath = relativePath.substr(rootPos);
            
            if (HtmlBuilder::hasMergedAssetInCache(relativePath)) {
                builtFile = HtmlBuilder::getMergedAssetFromCache(relativePath);
                return;
            }
        }
    }
    
    // Handle comment removal if enabled
    if (_serverData.getRemoveComments() && !builtFile.empty()) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_JS);
    }
    
    // Check if obfuscation should be applied
    // (only if not dev mode and obfuscation level > 0)
    if (!_serverData.shouldObfuscate()) {
        // No obfuscation needed
        return;
    }
    
    // Check if we have a valid cached obfuscated version
    std::string cacheFilePath = path;
    size_t jsPos = cacheFilePath.rfind(".js");
    if (jsPos != std::string::npos) {
        cacheFilePath.replace(jsPos, 3, ".obfuscated.js");
    } else {
        cacheFilePath += ".obfuscated";
    }
    
    if (hasValidObfuscationCache(path)) {
        // Load from cache
        try {
            builtFile = ContentBuilder::loadFile(cacheFilePath);
        } catch (const std::exception&) {
            // Failed to load cache - fall through to re-obfuscate
        }
    } else {
        // Need to obfuscate and cache
        try {
            builtFile = obfuscateAndCache(builtFile, cacheFilePath);
        } catch (const std::exception&) {
            // Obfuscation failed - builtFile already contains original content
        }
    }
}

bool JSBuilder::isExcludedFromObfuscation(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string filename = p.filename().string();
    return _serverData.isObfuscationExcluded(filename);
}

bool JSBuilder::hasValidObfuscationCache(const std::string& filePath) {
    namespace fs = std::filesystem;
    
    // Generate cache file path: replace .js with .obfuscated.js
    std::string cacheFilePath = filePath;
    size_t jsPos = cacheFilePath.rfind(".js");
    if (jsPos != std::string::npos) {
        cacheFilePath.replace(jsPos, 3, ".obfuscated.js");
    } else {
        cacheFilePath += ".obfuscated";
    }
    
    // Check if both source and cache files exist
    if (!fs::exists(filePath) || !fs::exists(cacheFilePath)) {
        return false;
    }
    
    try {
        // Get modification times
        auto sourceTime = fs::last_write_time(filePath);
        auto cacheTime = fs::last_write_time(cacheFilePath);
        auto now = fs::file_time_type::clock::now();
        
        // Cache is invalid if source is newer than cache
        if (sourceTime > cacheTime) {
            return false;
        }
        
        // Calculate cache age in days
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - cacheTime).count() / 24;
        
        // Check if within expiry time
        int expiryDays = _serverData.getObfuscationCacheExpiry();
        return age < expiryDays;
        
    } catch (const std::exception&) {
        // Error checking cache validity - consider invalid
        return false;
    }
}

std::string JSBuilder::obfuscateAndCache(const std::string& content, const std::string& cacheFilePath) {
    // Create obfuscator with configured level
    JSObfuscator obfuscator(_serverData.getObfuscationLevel());
    
    // Obfuscate the content
    std::string obfuscated = obfuscator.obfuscate(content);
    
    // Save to disk cache file (e.g., utils.obfuscated.js)
    try {
        FileManagement::saveFile(cacheFilePath, obfuscated);
    } catch (const std::exception&) {
        // Failed to cache - continue anyway
    }
    
    return obfuscated;
}

}  // namespace geruest
