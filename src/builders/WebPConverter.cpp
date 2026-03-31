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

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace geruest {

// StbiDeleter body defined here where stbi_image_free is visible
void WebPConverter::StbiDeleter::operator()(uint8_t* p) const noexcept {
    if (p) stbi_image_free(p);
}

// Initialize static members
std::mutex WebPConverter::_staticCacheMutex;
std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> WebPConverter::_staticWebpCache;
std::deque<std::string> WebPConverter::_staticCacheOrder;
size_t WebPConverter::_staticCacheSizeBytes = 0;
size_t WebPConverter::_staticMaxCacheBytes = WebPConverter::WEBP_DEFAULT_MAX_CACHE_BYTES;
std::condition_variable WebPConverter::_conversionCV;
std::unordered_set<std::string> WebPConverter::_inProgressConversions;
int WebPConverter::_maxConversionDimension = WebPConverter::WEBP_DEFAULT_MAX_DIMENSION;

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

std::shared_ptr<const std::vector<uint8_t>> WebPConverter::getFromCache(const std::string& webpPath) {
    std::lock_guard<std::mutex> lock(_staticCacheMutex);
    auto it = _staticWebpCache.find(webpPath);
    if (it != _staticWebpCache.end()) {
        return it->second;
    }
    return nullptr;
}

bool WebPConverter::hasInCache(const std::string& webpPath) {
    std::lock_guard<std::mutex> lock(_staticCacheMutex);
    return _staticWebpCache.find(webpPath) != _staticWebpCache.end();
}

void WebPConverter::clearStaticCache() {
    {
        std::lock_guard<std::mutex> lock(_staticCacheMutex);
        _staticWebpCache.clear();
        _staticCacheOrder.clear();
        _staticCacheSizeBytes = 0;
        // _inProgressConversions is intentionally NOT cleared here:
        // any thread currently mid-conversion must still finish and clean up.
    }
    _conversionCV.notify_all();
}

void WebPConverter::setMaxCacheBytes(size_t maxBytes) {
    std::lock_guard<std::mutex> lock(_staticCacheMutex);
    _staticMaxCacheBytes = maxBytes;
}

void WebPConverter::setMaxConversionDimension(int maxDimension) {
    _maxConversionDimension = (maxDimension < 0) ? 0 : maxDimension;
}

// ========== Static HTML processing methods ==========

std::vector<std::string> WebPConverter::extractImagePathsFromHtml(const std::string& htmlContent) {
    std::vector<std::string> imagePaths;
    
    // Static regex patterns - compiled only once
    static const std::regex imgSrcRegex(R"(src\s*=\s*["']([^"']*\.(?:png|jpg|jpeg))["'])", std::regex::icase);
    static const std::regex bgImageRegex(R"(url\s*\(\s*["']?([^"')]*\.(?:png|jpg|jpeg))["']?\s*\))", std::regex::icase);
    
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
    
    // Static regex patterns - compiled only once for performance
    static const std::regex srcPngRegex(R"((src\s*=\s*["'][^"']*\.)png(["']))", std::regex::icase);
    static const std::regex srcJpgRegex(R"((src\s*=\s*["'][^"']*\.)jpg(["']))", std::regex::icase);
    static const std::regex srcJpegRegex(R"((src\s*=\s*["'][^"']*\.)jpeg(["']))", std::regex::icase);
    static const std::regex bgPngRegex(R"((url\s*\(\s*["']?[^"')]*\.)png(["']?\s*\)))", std::regex::icase);
    static const std::regex bgJpgRegex(R"((url\s*\(\s*["']?[^"')]*\.)jpg(["']?\s*\)))", std::regex::icase);
    static const std::regex bgJpegRegex(R"((url\s*\(\s*["']?[^"')]*\.)jpeg(["']?\s*\)))", std::regex::icase);
    
    // Replace extensions in src attributes
    result = std::regex_replace(result, srcPngRegex, "$1webp$2");
    result = std::regex_replace(result, srcJpgRegex, "$1webp$2");
    result = std::regex_replace(result, srcJpegRegex, "$1webp$2");
    
    // Replace extensions in CSS url()
    result = std::regex_replace(result, bgPngRegex, "$1webp$2");
    result = std::regex_replace(result, bgJpgRegex, "$1webp$2");
    result = std::regex_replace(result, bgJpegRegex, "$1webp$2");
    
    return result;
}

// ========== Image loading, resizing and encoding ==========

std::vector<uint8_t> WebPConverter::resizeImage(StbiBuffer src,
                                                  int& width, int& height,
                                                  int channels, int maxDim) {
    // Compute target dimensions preserving aspect ratio
    const float scale = static_cast<float>(maxDim) /
                        static_cast<float>((width >= height) ? width : height);
    int dstW = static_cast<int>(static_cast<float>(width)  * scale);
    int dstH = static_cast<int>(static_cast<float>(height) * scale);
    if (dstW < 1) dstW = 1;
    if (dstH < 1) dstH = 1;

    std::cerr << "WebPConverter: Resizing " << width << "x" << height
              << " -> " << dstW << "x" << dstH
              << " (~" << (static_cast<size_t>(dstW) * static_cast<size_t>(dstH)
                           * static_cast<size_t>(channels) / (1024 * 1024))
              << " MB after resize)" << std::endl;

    std::vector<uint8_t> resized(
        static_cast<size_t>(dstW) * static_cast<size_t>(dstH) *
        static_cast<size_t>(channels));

    const stbir_pixel_layout layout = (channels == 4) ? STBIR_RGBA : STBIR_RGB;

    stbir_resize_uint8_linear(
        src.get(),      width, height, 0,
        resized.data(), dstW,  dstH,   0,
        layout);

    // Free the original large stbi buffer immediately — peak was src+resized,
    // now only the small resized vector remains.
    src.reset();

    width  = dstW;
    height = dstH;
    return resized;
}

WebPConverter::StbiBuffer WebPConverter::loadImage(const std::string& path,
                                                    int& width, int& height, int& channels) {
    // Peek at the file header to find actual dimensions and channel count.
    // This lets us log memory usage and choose RGB vs RGBA before allocating.
    int fileChannels = 0;
    if (!stbi_info(path.c_str(), &width, &height, &fileChannels)) {
        std::cerr << "WebPConverter: Failed to read image info: " << path << std::endl;
        return nullptr;
    }

    // Use 4 channels (RGBA) only when the source actually has alpha.
    // All JPEGs and most PNGs are 3-channel (RGB), saving 25 % of buffer size.
    channels = (fileChannels == 4) ? 4 : 3;

    const size_t rawBytes =
        static_cast<size_t>(width) * static_cast<size_t>(height) *
        static_cast<size_t>(channels);
    std::cerr << "WebPConverter: Loading " << path
              << " (" << width << "x" << height
              << ", " << channels << "ch"
              << ", ~" << (rawBytes / (1024 * 1024)) << " MB raw)" << std::endl;

    // stbi_load returns its own allocation; we wrap it in StbiBuffer so it is
    // freed via stbi_image_free — no copy is made here.
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &fileChannels, channels);
    if (!data) {
        std::cerr << "WebPConverter: Failed to load image: " << path << std::endl;
        std::cerr << "  Reason: " << stbi_failure_reason() << std::endl;
        return nullptr;
    }

    return StbiBuffer(data);
}

std::vector<uint8_t> WebPConverter::encodeToWebP(const uint8_t* pixels,
                                                   int width, int height,
                                                   int channels, float quality) {
#if GERUEST_HAS_WEBP
    uint8_t* output = nullptr;
    size_t outputSize = 0;

    if (channels == 4) {
        outputSize = WebPEncodeRGBA(pixels, width, height, width * 4, quality, &output);
    } else {
        // RGB path: ~25 % less memory inside libwebp than RGBA
        outputSize = WebPEncodeRGB(pixels, width, height, width * 3, quality, &output);
    }

    if (outputSize == 0 || output == nullptr) {
        std::cerr << "WebPConverter: Failed to encode WebP" << std::endl;
        return {};
    }

    std::vector<uint8_t> result(output, output + outputSize);
    WebPFree(output);
    return result;
#else
    (void)pixels;
    (void)width;
    (void)height;
    (void)channels;
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
    if (!fs::exists(sourcePath)) {
        std::cerr << "WebPConverter: Source file not found: " << sourcePath << std::endl;
        return false;
    }
    if (!isConvertibleImage(sourcePath)) {
        std::cerr << "WebPConverter: Not a convertible image: " << sourcePath << std::endl;
        return false;
    }

    if (cacheOnly) {
        // --- In-flight deduplication for cache-only (dev) mode ---
        //
        // Under _staticCacheMutex we atomically check the cache, and if the
        // image is already being converted by another thread we wait on
        // _conversionCV until that thread finishes.  This guarantees:
        //  • Each image path is decoded/encoded by at most ONE thread at a time,
        //    bounding peak memory to a single RGBA + WebP buffer set.
        //  • No TOCTOU race: the check and the "claim" are atomic.
        //  • Different image paths can still progress independently.
        {
            std::unique_lock<std::mutex> lock(_staticCacheMutex);
            _conversionCV.wait(lock, [&outputPath]() {
                return _staticWebpCache.count(outputPath) > 0 ||
                       _inProgressConversions.count(outputPath) == 0;
            });
            if (_staticWebpCache.count(outputPath) > 0) {
                return true; // another thread already converted it
            }
            _inProgressConversions.insert(outputPath); // we own this conversion
        }
    }

    // Decode + encode with no lock held so other images can be claimed in parallel.
    //
    // Memory model (zero-copy load path):
    //   loadImage  → StbiBuffer (stbi's own allocation, no vector copy)
    //   resizeImage→ takes ownership of StbiBuffer, allocates small target
    //                vector, frees StbiBuffer immediately → peak = src + dst
    //   encodeToWebP→ takes raw pointer; after this call only the WebP output
    //                remains in memory
    int width, height, channels;
    StbiBuffer srcBuf = loadImage(sourcePath, width, height, channels);

    if (!srcBuf) {
        if (cacheOnly) {
            { std::lock_guard<std::mutex> lock(_staticCacheMutex); _inProgressConversions.erase(outputPath); }
            _conversionCV.notify_all();
        }
        return false;
    }

    // Downscale if needed.  resizeImage moves srcBuf in and resets it before
    // returning, so the large source allocation is freed immediately.
    // If no resize is needed, encode directly from the stbi buffer (no copy).
    std::vector<uint8_t> resizedPixels;
    const uint8_t* encodePtr = nullptr;

    if (_maxConversionDimension > 0 &&
        (width > _maxConversionDimension || height > _maxConversionDimension)) {
        resizedPixels = resizeImage(std::move(srcBuf), width, height, channels,
                                    _maxConversionDimension);
        encodePtr = resizedPixels.data();
    } else {
        // No resize: encode straight from the stbi buffer — zero extra copy.
        encodePtr = srcBuf.get();
    }

    std::vector<uint8_t> webpData = encodeToWebP(encodePtr, width, height, channels, quality);

    // Release all pixel memory before touching the cache
    srcBuf.reset();
    resizedPixels.clear();
    resizedPixels.shrink_to_fit();

    if (webpData.empty()) {
        if (cacheOnly) {
            { std::lock_guard<std::mutex> lock(_staticCacheMutex); _inProgressConversions.erase(outputPath); }
            _conversionCV.notify_all();
        }
        return false;
    }

    if (cacheOnly) {
        auto entry = std::make_shared<const std::vector<uint8_t>>(std::move(webpData));
        const size_t entrySize = entry->size();

        {
            std::lock_guard<std::mutex> lock(_staticCacheMutex);

            if (entrySize <= _staticMaxCacheBytes) {
                // Evict oldest entries until the new entry fits
                while (!_staticCacheOrder.empty() &&
                       _staticCacheSizeBytes + entrySize > _staticMaxCacheBytes) {
                    const std::string& oldest = _staticCacheOrder.front();
                    auto it = _staticWebpCache.find(oldest);
                    if (it != _staticWebpCache.end()) {
                        _staticCacheSizeBytes -= it->second->size();
                        _staticWebpCache.erase(it);
                    }
                    _staticCacheOrder.pop_front();
                }
                _staticCacheSizeBytes += entrySize;
                _staticWebpCache[outputPath] = std::move(entry);
                _staticCacheOrder.push_back(outputPath);
            } else {
                std::cerr << "WebPConverter: entry too large for cache ("
                          << entrySize << " bytes), serving without caching: "
                          << outputPath << std::endl;
            }

            _inProgressConversions.erase(outputPath);
        }
        // Notify outside the lock so waiting threads re-check the predicate
        _conversionCV.notify_all();
        return true;
    } else {
        return saveWebP(outputPath, webpData);
    }
#else
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
