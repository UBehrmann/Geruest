/**
 * @file DevAssetCache.hpp
 * @brief Per-server in-memory cache for dev-mode merged assets and WebP images.
 */

#ifndef GERUEST_DEVASSETCACHE_HPP
#define GERUEST_DEVASSETCACHE_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace geruest {

/**
 * Dev-mode cache for merged CSS/JS bundles and WebP bytes.
 * Merged assets are keyed by URL-relative paths (/assets/css/page.css).
 * WebP entries are keyed by absolute filesystem paths.
 */
class DevAssetCache {
   public:
    static constexpr size_t DEFAULT_MAX_WEBP_BYTES = 64ULL * 1024 * 1024;

    DevAssetCache() = default;
    DevAssetCache(const DevAssetCache&) = delete;
    DevAssetCache& operator=(const DevAssetCache&) = delete;

    void putMergedAsset(const std::string& relativePath, std::string content) const;
    bool hasMergedAsset(const std::string& relativePath) const;
    std::string getMergedAsset(const std::string& relativePath) const;

    void putWebP(const std::string& absolutePath,
                 std::shared_ptr<const std::vector<uint8_t>> data) const;
    bool hasWebP(const std::string& absolutePath) const;
    std::shared_ptr<const std::vector<uint8_t>> getWebP(const std::string& absolutePath) const;

    void setMaxWebPBytes(size_t maxBytes) const;
    void clear() const;

   private:
    mutable std::mutex _mutex;
    mutable std::unordered_map<std::string, std::string> _merged;
    mutable std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> _webp;
    mutable std::deque<std::string> _webpOrder;
    mutable size_t _webpSizeBytes = 0;
    mutable size_t _maxWebpBytes = DEFAULT_MAX_WEBP_BYTES;
};

}  // namespace geruest

#endif  // GERUEST_DEVASSETCACHE_HPP
