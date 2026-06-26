/**
 * @file TextResponseCache.cpp
 */

#include "TextResponseCache.hpp"

#include "HTTPRequest.hpp"
#include "handler/StaticHttpCache.hpp"

#include <filesystem>

namespace geruest {

std::string TextResponseCache::makeKey(const std::string& contentType, const std::string& contentPath) {
    return contentType + "|" + contentPath;
}

TextCacheLookup TextResponseCache::lookup(const std::string& key, const std::string& contentPath,
                                          const HTTPRequest* request, bool devMode, size_t maxEntryBytes,
                                          size_t maxTotalBytes) const {
    TextCacheLookup result;
    if (devMode || maxEntryBytes == 0 || maxTotalBytes == 0) {
        return result;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _entries.find(key);
    if (it == _entries.end()) {
        return result;
    }
    if (!it->second.hasMtime) {
        _totalBytes -= it->second.sizeBytes;
        _entries.erase(it);
        return result;
    }

    std::error_code ec;
    const auto currentMtime = std::filesystem::last_write_time(contentPath, ec);
    if (ec || currentMtime != it->second.mtime) {
        _totalBytes -= it->second.sizeBytes;
        _entries.erase(it);
        return result;
    }

    if (request != nullptr) {
        StaticCacheHeaders headers;
        headers.etag = it->second.etag;
        headers.lastModified = it->second.lastModified;
        if (matchesNotModified(*request, headers)) {
            result.notModified = true;
            result.etag = it->second.etag;
            result.lastModified = it->second.lastModified;
            return result;
        }
    }

    result.payload = it->second.payload;
    return result;
}

void TextResponseCache::store(const std::string& key, const std::string& contentPath, const std::string& payload,
                              const std::string& etag, const std::string& lastModified, bool devMode,
                              size_t maxEntryBytes, size_t maxTotalBytes) const {
    if (devMode || payload.empty() || maxEntryBytes == 0 || maxTotalBytes == 0 || payload.size() > maxEntryBytes) {
        return;
    }

    Entry entry;
    entry.payload = std::make_shared<const std::string>(payload);
    entry.sizeBytes = payload.size();
    entry.etag = etag;
    entry.lastModified = lastModified;
    std::error_code ec;
    entry.mtime = std::filesystem::last_write_time(contentPath, ec);
    entry.hasMtime = !ec;

    std::lock_guard<std::mutex> lock(_mutex);
    if (const auto existing = _entries.find(key); existing != _entries.end()) {
        _totalBytes -= existing->second.sizeBytes;
        _entries.erase(existing);
    }
    while (_totalBytes + entry.sizeBytes > maxTotalBytes && !_entries.empty()) {
        auto victim = _entries.begin();
        _totalBytes -= victim->second.sizeBytes;
        _entries.erase(victim);
    }
    _totalBytes += entry.sizeBytes;
    _entries.emplace(key, std::move(entry));
}

}  // namespace geruest
