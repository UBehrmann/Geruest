/**
 * @file TextResponseCache.cpp
 */

#include "TextResponseCache.hpp"

#include <filesystem>

namespace geruest {

std::string TextResponseCache::makeKey(const std::string& contentType, const std::string& contentPath) {
    return contentType + "|" + contentPath;
}

std::shared_ptr<const std::string> TextResponseCache::lookup(const std::string& key, const std::string& contentPath,
                                                               bool devMode, size_t maxEntryBytes,
                                                               size_t maxTotalBytes) const {
    if (devMode || maxEntryBytes == 0 || maxTotalBytes == 0) {
        return {};
    }
    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _entries.find(key);
    if (it == _entries.end()) {
        return {};
    }
    if (!it->second.hasMtime) {
        _totalBytes -= it->second.sizeBytes;
        _entries.erase(it);
        return {};
    }
    std::error_code ec;
    const auto currentMtime = std::filesystem::last_write_time(contentPath, ec);
    if (ec || currentMtime != it->second.mtime) {
        _totalBytes -= it->second.sizeBytes;
        _entries.erase(it);
        return {};
    }
    return it->second.payload;
}

void TextResponseCache::store(const std::string& key, const std::string& contentPath, const std::string& payload,
                              bool devMode, size_t maxEntryBytes, size_t maxTotalBytes) const {
    if (devMode || payload.empty() || maxEntryBytes == 0 || maxTotalBytes == 0 || payload.size() > maxEntryBytes) {
        return;
    }

    Entry entry;
    entry.payload = std::make_shared<const std::string>(payload);
    entry.sizeBytes = payload.size();
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
