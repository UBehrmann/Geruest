/**
 * @file TextResponseCache.cpp
 */

#include "TextResponseCache.hpp"

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace geruest {

namespace {

struct TextResponseCacheEntry {
    std::shared_ptr<const std::string> payload;
    std::filesystem::file_time_type mtime{};
    bool hasMtime = false;
    size_t sizeBytes = 0;
};

struct TextResponseCacheState {
    std::mutex mutex;
    std::unordered_map<std::string, TextResponseCacheEntry> entries;
    size_t totalBytes = 0;
};

TextResponseCacheState& cacheState() {
    static TextResponseCacheState state;
    return state;
}

}  // namespace

TextResponseCache& TextResponseCache::instance() {
    static TextResponseCache cache;
    return cache;
}

std::string TextResponseCache::makeKey(const std::string& contentType, const std::string& contentPath) {
    return contentType + "|" + contentPath;
}

std::shared_ptr<const std::string> TextResponseCache::lookup(const std::string& key, bool devMode,
                                                               size_t maxEntryBytes, size_t maxTotalBytes) {
    if (devMode || maxEntryBytes == 0 || maxTotalBytes == 0) {
        return {};
    }
    auto& state = cacheState();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto it = state.entries.find(key);
    if (it == state.entries.end()) {
        return {};
    }
    return it->second.payload;
}

void TextResponseCache::store(const std::string& key, const std::string& contentPath, const std::string& payload,
                              bool devMode, size_t maxEntryBytes, size_t maxTotalBytes) {
    if (devMode || payload.empty() || maxEntryBytes == 0 || maxTotalBytes == 0 || payload.size() > maxEntryBytes) {
        return;
    }

    TextResponseCacheEntry entry;
    entry.payload = std::make_shared<const std::string>(payload);
    entry.sizeBytes = payload.size();
    std::error_code ec;
    entry.mtime = std::filesystem::last_write_time(contentPath, ec);
    entry.hasMtime = !ec;

    auto& state = cacheState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (const auto existing = state.entries.find(key); existing != state.entries.end()) {
        state.totalBytes -= existing->second.sizeBytes;
        state.entries.erase(existing);
    }
    while (state.totalBytes + entry.sizeBytes > maxTotalBytes && !state.entries.empty()) {
        auto victim = state.entries.begin();
        state.totalBytes -= victim->second.sizeBytes;
        state.entries.erase(victim);
    }
    state.totalBytes += entry.sizeBytes;
    state.entries.emplace(key, std::move(entry));
}

}  // namespace geruest
