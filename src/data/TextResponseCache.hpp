/**
 * @file TextResponseCache.hpp
 * @brief Per-server cache for serialized text responses (HTML/CSS/JS).
 */

#ifndef GERUEST_TEXTRESPONSECACHE_HPP
#define GERUEST_TEXTRESPONSECACHE_HPP

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace geruest {

class HTTPRequest;

struct TextCacheLookup {
    bool notModified = false;
    std::shared_ptr<const std::string> payload;
    std::string etag;
    std::string lastModified;
};

class TextResponseCache {
   public:
    TextResponseCache() = default;
    TextResponseCache(const TextResponseCache&) = delete;
    TextResponseCache& operator=(const TextResponseCache&) = delete;

    static std::string makeKey(const std::string& contentType, const std::string& contentPath);

    TextCacheLookup lookup(const std::string& key, const std::string& contentPath, const HTTPRequest* request,
                           bool devMode, size_t maxEntryBytes, size_t maxTotalBytes) const;

    void store(const std::string& key, const std::string& contentPath, const std::string& payload,
               const std::string& etag, const std::string& lastModified, bool devMode, size_t maxEntryBytes,
               size_t maxTotalBytes) const;

   private:
    struct Entry {
        std::shared_ptr<const std::string> payload;
        std::filesystem::file_time_type mtime{};
        bool hasMtime = false;
        size_t sizeBytes = 0;
        std::string etag;
        std::string lastModified;
    };

    mutable std::mutex _mutex;
    mutable std::unordered_map<std::string, Entry> _entries;
    mutable size_t _totalBytes = 0;
};

}  // namespace geruest

#endif  // GERUEST_TEXTRESPONSECACHE_HPP
