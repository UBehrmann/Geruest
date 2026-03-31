/**
 * @file WebPConverter.hpp
 * @date 02.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief WebP image conversion utility for automatic PNG/JPEG to WebP conversion.
 * 
 * This class provides functionality to convert PNG and JPEG images to WebP format,
 * either storing them on disk (production mode) or caching them in memory (dev mode).
 */

#ifndef GERUEST_WEBPCONVERTER_HPP
#define GERUEST_WEBPCONVERTER_HPP

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdint>

namespace geruest {

/**
 * @brief WebP conversion utility class
 * 
 * Handles conversion of PNG and JPEG images to WebP format.
 * Supports both production mode (saves to disk) and development mode (in-memory cache).
 */
class WebPConverter {
public:
    /**
     * @brief Default WebP quality (0-100, where 100 is lossless)
     */
    static constexpr float WEBP_DEFAULT_QUALITY = 75.0f;

    /**
     * @brief Default maximum in-memory cache size (64 MB).
     *        Oldest entries are evicted once this limit is reached.
     */
    static constexpr size_t WEBP_DEFAULT_MAX_CACHE_BYTES = 64ULL * 1024 * 1024;

    // ========== Static methods for direct use without instance ==========

    /**
     * @brief Convert an image file to WebP format (static version)
     * @param sourcePath The path to the source image (PNG, JPEG, or JPG)
     * @param outputPath The path where the WebP file should be saved
     * @param cacheOnly If true, store in memory cache only (don't save to disk)
     * @param quality WebP compression quality (0-100)
     * @return true if conversion succeeded, false otherwise
     */
    static bool convertImage(const std::string& sourcePath, const std::string& outputPath, 
                            bool cacheOnly = false, float quality = WEBP_DEFAULT_QUALITY);

    /**
     * @brief Extract image paths from HTML content (returns vector of relative paths)
     * @param htmlContent The HTML content to scan
     * @return Vector of relative paths to images found in the HTML
     */
    static std::vector<std::string> extractImagePathsFromHtml(const std::string& htmlContent);

    /**
     * @brief Replace image references in HTML content with WebP equivalents
     * Images are expected to be in assets/images/ directory (standard path)
     * @param htmlContent The HTML content to modify
     * @return Modified HTML with .webp extensions and correct paths
     */
    static std::string replaceImageReferencesWithWebP(const std::string& htmlContent);

    /**
     * @brief Get WebP data from the static in-memory cache (zero-copy).
     * @param webpPath The path of the WebP file
     * @return Shared pointer to the WebP binary data, or nullptr if not found
     */
    static std::shared_ptr<const std::vector<uint8_t>> getFromCache(const std::string& webpPath);

    /**
     * @brief Set the maximum total byte size of the in-memory cache.
     *        Oldest entries are evicted to stay within the limit.
     * @param maxBytes Maximum cache size in bytes
     */
    static void setMaxCacheBytes(size_t maxBytes);

    /**
     * @brief Check if a WebP image exists in static cache
     * @param webpPath The path of the WebP file
     * @return true if the image is in cache
     */
    static bool hasInCache(const std::string& webpPath);

    /**
     * @brief Clear the static in-memory WebP cache
     */
    static void clearStaticCache();

    /**
     * @brief Get the WebP path for a given source image path
     * @param sourcePath The path to the source PNG/JPEG image
     * @return The corresponding WebP path
     */
    static std::string getWebPPath(const std::string& sourcePath);

    /**
     * @brief Check if a file path is a convertible image (PNG, JPEG, JPG)
     * @param path The file path to check
     * @return true if the file can be converted to WebP
     */
    static bool isConvertibleImage(const std::string& path);

    // ========== Instance methods for batch operations ==========

    /**
     * @brief Construct a new WebPConverter
     * @param serverRoot The root directory of the server's website files
     * @param devMode If true, converted images are kept in memory only
     * @param quality WebP compression quality (0-100)
     */
    WebPConverter(const std::string& serverRoot, bool devMode = false, float quality = WEBP_DEFAULT_QUALITY);

    /**
     * @brief Convert an image file to WebP format
     * @param imagePath The path to the source image (PNG, JPEG, or JPG)
     * @return true if conversion succeeded, false otherwise
     */
    bool convertImageInstance(const std::string& imagePath);

    /**
     * @brief Convert multiple images to WebP format
     * @param imagePaths Vector of paths to source images
     * @return Number of successfully converted images
     */
    size_t convertImages(const std::vector<std::string>& imagePaths);

    /**
     * @brief Set the WebP quality
     * @param quality Quality value (0-100)
     */
    void setQuality(float quality);

    /**
     * @brief Get the current WebP quality setting
     * @return Current quality value
     */
    float getQuality() const { return _quality; }

private:
    std::string _serverRoot;
    bool _devMode;
    float _quality;

    // Static cache for WebP images (shared across all instances).
    // Values are shared_ptr so callers can hold a reference without copying.
    // A parallel deque tracks insertion order for FIFO eviction.
    static std::mutex _staticCacheMutex;
    static std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> _staticWebpCache;
    static std::deque<std::string> _staticCacheOrder;
    static size_t _staticCacheSizeBytes;
    static size_t _staticMaxCacheBytes;

    /**
     * @brief Load a PNG or JPEG file into RGBA buffer using stb_image
     * @param path Path to the image file
     * @param width Output: Image width
     * @param height Output: Image height
     * @return RGBA pixel data, or empty vector on failure
     */
    static std::vector<uint8_t> loadImage(const std::string& path, int& width, int& height);

    /**
     * @brief Encode RGBA data to WebP format
     * @param rgba RGBA pixel data
     * @param width Image width
     * @param height Image height
     * @param quality WebP quality (0-100)
     * @return WebP binary data, or empty vector on failure
     */
    static std::vector<uint8_t> encodeToWebP(const std::vector<uint8_t>& rgba, int width, int height, float quality);

    /**
     * @brief Save WebP data to a file
     * @param path Output file path
     * @param data WebP binary data
     * @return true if save succeeded
     */
    static bool saveWebP(const std::string& path, const std::vector<uint8_t>& data);
};

}  // namespace geruest

#endif  // GERUEST_WEBPCONVERTER_HPP
