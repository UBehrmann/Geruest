/**
 * @file TextResponseCache.hpp
 * @brief Process-wide cache for serialized text responses (HTML/CSS/JS).
 */

#ifndef GERUEST_TEXTRESPONSECACHE_HPP
#define GERUEST_TEXTRESPONSECACHE_HPP

#include <memory>
#include <string>

namespace geruest {

class TextResponseCache {
   public:
    static TextResponseCache& instance();

    static std::string makeKey(const std::string& contentType, const std::string& contentPath);

    std::shared_ptr<const std::string> lookup(const std::string& key, bool devMode, size_t maxEntryBytes,
                                              size_t maxTotalBytes);

    void store(const std::string& key, const std::string& contentPath, const std::string& payload, bool devMode,
               size_t maxEntryBytes, size_t maxTotalBytes);

   private:
    TextResponseCache() = default;
};

}  // namespace geruest

#endif  // GERUEST_TEXTRESPONSECACHE_HPP
