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
#include <condition_variable>
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

    /**
     * @brief Default maximum pixel dimension (longest side) before downscaling.
     *        Images wider or taller than this are scaled down proportionally
     *        before encoding, dramatically reducing peak memory usage.
     *        Set to 0 to disable resizing.
     */
    static constexpr int WEBP_DEFAULT_MAX_DIMENSION = 1920;

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
     * @brief Set the maximum pixel dimension (longest side) for conversion.
     *        Images larger than this are downscaled proportionally before
     *        encoding.  This bounds peak decode/encode memory to a predictable
     *        value regardless of the source image size.
     *        Set to 0 to disable automatic resizing.
     * @param maxDimension Maximum width or height in pixels (default 1920)
     */
    static void setMaxConversionDimension(int maxDimension);

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
    // _staticCacheMutex also guards _inProgressConversions (see below).
    static std::mutex _staticCacheMutex;
    static std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> _staticWebpCache;
    static std::deque<std::string> _staticCacheOrder;
    static size_t _staticCacheSizeBytes;
    static size_t _staticMaxCacheBytes;

    // In-flight deduplication: tracks image paths that are currently being
    // decoded/encoded by some thread.  Any other thread that wants the same
    // path waits on _conversionCV instead of starting a second conversion.
    // This ensures each unique image is converted by at most one thread at a
    // time, bounding peak memory to a single decode+encode buffer set.
    static std::condition_variable _conversionCV;
    static std::unordered_set<std::string> _inProgressConversions;

    // Maximum pixel dimension (longest side) before downscaling.  0 = disabled.
    static int _maxConversionDimension;

    /**
     * @brief Return the number of bytes currently available to this process.
     *        Reads the Docker/cgroup memory limit when running in a container
     *        so the estimate is correct even inside a memory-limited container.
     *        Returns 0 if the limit cannot be determined.
     */
    static size_t getAvailableMemoryBytes();

    /**
     * @brief Estimate peak memory (bytes) needed to convert an image.
     *        With resize: source buffer + resized buffer (source freed mid-way).
     *        Without resize: source buffer × 2.5 (libwebp working memory).
     */
    static size_t estimateConversionPeakBytes(int srcW, int srcH, int channels);

    // RAII wrapper for stbi_load output — avoids copying the raw pixel buffer.
    // The deleter body is defined in WebPConverter.cpp where stb_image.h lives.
    struct StbiDeleter { void operator()(uint8_t* p) const noexcept; };
    using StbiBuffer = std::unique_ptr<uint8_t[], StbiDeleter>;

    /**
     * @brief Load a PNG or JPEG file using stb_image.
     *        JPEGs load as RGB (3 ch); PNGs with alpha load as RGBA (4 ch).
     *        The returned buffer is the raw stbi allocation — no copy is made.
     * @param path     Path to the image file
     * @param width    Output: image width in pixels
     * @param height   Output: image height in pixels
     * @param channels Output: number of channels loaded (3 or 4)
     * @return Owning stbi buffer, or nullptr on failure
     */
    static StbiBuffer loadImage(const std::string& path,
                                int& width, int& height, int& channels);

    /**
     * @brief Downscale a pixel buffer so its longest side is at most maxDim.
     *        Aspect ratio is preserved.  Takes ownership of the source buffer
     *        and frees it immediately after writing the resized output, so
     *        peak memory = source + target (not source + source + target).
     * @param src      Source stbi buffer (consumed / freed inside)
     * @param width    In/out: source width → target width
     * @param height   In/out: source height → target height
     * @param channels Number of channels (3 or 4)
     * @param maxDim   Maximum allowed dimension on the longest side
     * @return Resized pixel data as a vector
     */
    static std::vector<uint8_t> resizeImage(StbiBuffer src,
                                             int& width, int& height,
                                             int channels, int maxDim);

    /**
     * @brief Encode a raw pixel buffer to WebP format.
     * @param pixels   Pointer to RGB or RGBA pixel data
     * @param width    Image width
     * @param height   Image height
     * @param channels 3 (RGB) or 4 (RGBA)
     * @param quality  WebP quality (0-100)
     * @return WebP binary data, or empty vector on failure
     */
    static std::vector<uint8_t> encodeToWebP(const uint8_t* pixels,
                                              int width, int height,
                                              int channels, float quality);

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
