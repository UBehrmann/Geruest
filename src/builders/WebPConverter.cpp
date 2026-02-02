/**
 * @file WebPConverter.cpp
 * @date 02.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief WebP image conversion implementation.
 * 
 * Uses libwebp for encoding. Uses stb_image for decoding PNG/JPEG source images.
 */

#include "WebPConverter.hpp"
#include "FileManagement/FileManagement.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <filesystem>

// WebP encoding library (optional)
#if GERUEST_HAS_WEBP
#include <webp/encode.h>
#endif

// STB Image for decoding PNG/JPEG (always available as fallback)
// Disable warnings for third-party header
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)  // conversion, possible loss of data
#pragma warning(disable: 4267)  // conversion from size_t
#pragma warning(disable: 4996)  // deprecated functions
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace geruest {

// Initialize static members
std::mutex WebPConverter::_staticCacheMutex;
std::unordered_map<std::string, std::vector<uint8_t>> WebPConverter::_staticWebpCache;

// ========== Constructor ==========

WebPConverter::WebPConverter(const std::string& serverRoot, bool devMode, float quality)
    : _serverRoot(serverRoot), _devMode(devMode), _quality(quality) {
    if (_quality < 0.0f) _quality = 0.0f;
    if (_quality > 100.0f) _quality = 100.0f;
}

void WebPConverter::setQuality(float quality) {
    _quality = quality;
    if (_quality < 0.0f) _quality = 0.0f;
    if (_quality > 100.0f) _quality = 100.0f;
}

// ========== Static utility methods ==========

bool WebPConverter::isConvertibleImage(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) return false;
    
    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return (ext == "png" || ext == "jpg" || ext == "jpeg");
}

std::string WebPConverter::getWebPPath(const std::string& sourcePath) {
    size_t dotPos = sourcePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return sourcePath + ".webp";
    }
    return sourcePath.substr(0, dotPos) + ".webp";
}

// ========== Static cache methods ==========

std::vector<uint8_t> WebPConverter::getFromCache(const std::string& webpPath) {
    std::lock_guard<std::mutex> lock(_staticCacheMutex);
    auto it = _staticWebpCache.find(webpPath);
    if (it != _staticWebpCache.end()) {
        return it->second;
    }
    return {};
}

bool WebPConverter::hasInCache(const std::string& webpPath) {
    std::lock_guard<std::mutex> lock(_staticCacheMutex);
    return _staticWebpCache.find(webpPath) != _staticWebpCache.end();
}

void WebPConverter::clearStaticCache() {
    std::lock_guard<std::mutex> lock(_staticCacheMutex);
    _staticWebpCache.clear();
}

// ========== Static HTML processing methods ==========

std::vector<std::string> WebPConverter::extractImagePathsFromHtml(const std::string& htmlContent) {
    std::vector<std::string> imagePaths;
    
    // Regex patterns for src attributes with PNG/JPEG images
    // Matches: src="path.png", src='path.jpg', src="path.jpeg"
    std::regex imgSrcRegex(R"(src\s*=\s*["']([^"']*\.(?:png|jpg|jpeg))["'])", std::regex::icase);
    
    // Regex for CSS background-image with PNG/JPEG
    std::regex bgImageRegex(R"(url\s*\(\s*["']?([^"')]*\.(?:png|jpg|jpeg))["']?\s*\))", std::regex::icase);
    
    // Find all src attributes
    std::sregex_iterator srcIt(htmlContent.begin(), htmlContent.end(), imgSrcRegex);
    std::sregex_iterator srcEnd;
    
    while (srcIt != srcEnd) {
        std::string imgPath = (*srcIt)[1].str();
        
        // Skip external URLs
        if (!imgPath.empty() && 
            imgPath.find("http://") != 0 && 
            imgPath.find("https://") != 0 &&
            imgPath.find("data:") != 0) {
            // Remove leading slash for consistency
            if (imgPath[0] == '/') {
                imgPath = imgPath.substr(1);
            }
            imagePaths.push_back(imgPath);
        }
        ++srcIt;
    }
    
    // Find all background-image URLs
    std::sregex_iterator bgIt(htmlContent.begin(), htmlContent.end(), bgImageRegex);
    
    while (bgIt != srcEnd) {
        std::string imgPath = (*bgIt)[1].str();
        
        if (!imgPath.empty() && 
            imgPath.find("http://") != 0 && 
            imgPath.find("https://") != 0 &&
            imgPath.find("data:") != 0) {
            if (imgPath[0] == '/') {
                imgPath = imgPath.substr(1);
            }
            imagePaths.push_back(imgPath);
        }
        ++bgIt;
    }
    
    // Remove duplicates
    std::sort(imagePaths.begin(), imagePaths.end());
    imagePaths.erase(std::unique(imagePaths.begin(), imagePaths.end()), imagePaths.end());
    
    return imagePaths;
}

std::string WebPConverter::replaceImageReferencesWithWebP(const std::string& htmlContent) {
    std::string result = htmlContent;
    
    // Just change the extension from .png/.jpg/.jpeg to .webp
    // Keep the path exactly as it is in the HTML
    // The assets/images/ prefix is only used internally for file lookup
    
    // Replace .png with .webp in src attributes
    std::regex srcPngRegex(R"((src\s*=\s*["'][^"']*\.)png(["']))", std::regex::icase);
    result = std::regex_replace(result, srcPngRegex, "$1webp$2");
    
    // Replace .jpg with .webp in src attributes
    std::regex srcJpgRegex(R"((src\s*=\s*["'][^"']*\.)jpg(["']))", std::regex::icase);
    result = std::regex_replace(result, srcJpgRegex, "$1webp$2");
    
    // Replace .jpeg with .webp in src attributes
    std::regex srcJpegRegex(R"((src\s*=\s*["'][^"']*\.)jpeg(["']))", std::regex::icase);
    result = std::regex_replace(result, srcJpegRegex, "$1webp$2");
    
    // CSS url() for background images
    std::regex bgPngRegex(R"((url\s*\(\s*["']?[^"')]*\.)png(["']?\s*\)))", std::regex::icase);
    result = std::regex_replace(result, bgPngRegex, "$1webp$2");
    
    std::regex bgJpgRegex(R"((url\s*\(\s*["']?[^"')]*\.)jpg(["']?\s*\)))", std::regex::icase);
    result = std::regex_replace(result, bgJpgRegex, "$1webp$2");
    
    std::regex bgJpegRegex(R"((url\s*\(\s*["']?[^"')]*\.)jpeg(["']?\s*\)))", std::regex::icase);
    result = std::regex_replace(result, bgJpegRegex, "$1webp$2");
    
    return result;
}

// ========== Image loading and encoding ==========

std::vector<uint8_t> WebPConverter::loadImage(const std::string& path, int& width, int& height) {
    // Use stb_image to load PNG or JPEG
    int channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); // Force RGBA
    
    if (!data) {
        std::cerr << "WebPConverter: Failed to load image: " << path << std::endl;
        std::cerr << "  Reason: " << stbi_failure_reason() << std::endl;
        return {};
    }
    
    // Copy to vector
    size_t dataSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    std::vector<uint8_t> result(data, data + dataSize);
    
    stbi_image_free(data);
    return result;
}

std::vector<uint8_t> WebPConverter::encodeToWebP(const std::vector<uint8_t>& rgba, 
                                                   int width, int height, float quality) {
#if GERUEST_HAS_WEBP
    uint8_t* output = nullptr;
    
    size_t outputSize = WebPEncodeRGBA(
        rgba.data(),
        width,
        height,
        width * 4,  // stride
        quality,
        &output
    );
    
    if (outputSize == 0 || output == nullptr) {
        std::cerr << "WebPConverter: Failed to encode WebP" << std::endl;
        return {};
    }
    
    std::vector<uint8_t> result(output, output + outputSize);
    WebPFree(output);
    
    return result;
#else
    // WebP not available
    (void)rgba;
    (void)width;
    (void)height;
    (void)quality;
    std::cerr << "WebPConverter: WebP encoding not available (GERUEST_HAS_WEBP=0)" << std::endl;
    return {};
#endif
}

bool WebPConverter::saveWebP(const std::string& path, const std::vector<uint8_t>& data) {
    // Create directories if needed
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
        if (ec) {
            std::cerr << "WebPConverter: Failed to create directory: " << filePath.parent_path() << std::endl;
            return false;
        }
    }
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "WebPConverter: Failed to open file for writing: " << path << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

// ========== Main conversion method ==========

bool WebPConverter::convertImage(const std::string& sourcePath, const std::string& outputPath,
                                  bool cacheOnly, float quality) {
#if GERUEST_HAS_WEBP
    // Check if source exists
    if (!fs::exists(sourcePath)) {
        std::cerr << "WebPConverter: Source file not found: " << sourcePath << std::endl;
        return false;
    }
    
    // Check if it's a convertible image
    if (!isConvertibleImage(sourcePath)) {
        std::cerr << "WebPConverter: Not a convertible image: " << sourcePath << std::endl;
        return false;
    }
    
    // Load the source image
    int width, height;
    std::vector<uint8_t> rgba = loadImage(sourcePath, width, height);
    
    if (rgba.empty()) {
        return false;
    }
    
    // Encode to WebP
    std::vector<uint8_t> webpData = encodeToWebP(rgba, width, height, quality);
    
    if (webpData.empty()) {
        return false;
    }
    
    if (cacheOnly) {
        // Store in static cache only
        std::lock_guard<std::mutex> lock(_staticCacheMutex);
        _staticWebpCache[outputPath] = std::move(webpData);
        return true;
    } else {
        // Save to disk
        return saveWebP(outputPath, webpData);
    }
#else
    // WebP not available
    (void)sourcePath;
    (void)outputPath;
    (void)cacheOnly;
    (void)quality;
    std::cerr << "WebPConverter: WebP encoding not available (GERUEST_HAS_WEBP=0)" << std::endl;
    return false;
#endif
}

// ========== Instance methods ==========

bool WebPConverter::convertImageInstance(const std::string& imagePath) {
    std::string webpPath = getWebPPath(imagePath);
    return convertImage(imagePath, webpPath, _devMode, _quality);
}

size_t WebPConverter::convertImages(const std::vector<std::string>& imagePaths) {
    size_t successCount = 0;
    
    for (const auto& path : imagePaths) {
        if (convertImageInstance(path)) {
            successCount++;
        }
    }
    
    return successCount;
}

}  // namespace geruest
