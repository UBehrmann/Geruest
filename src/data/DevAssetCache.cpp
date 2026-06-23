/**
 * @file DevAssetCache.cpp
 * @brief Dev-mode merged asset and WebP in-memory cache.
 */

#include "DevAssetCache.hpp"

#include <iostream>
#include <utility>

namespace geruest {

void DevAssetCache::putMergedAsset(const std::string& relativePath, std::string content) const {
    std::lock_guard<std::mutex> lock(_mutex);
    _merged[relativePath] = std::move(content);
}

bool DevAssetCache::hasMergedAsset(const std::string& relativePath) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _merged.find(relativePath) != _merged.end();
}

std::string DevAssetCache::getMergedAsset(const std::string& relativePath) const {
    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _merged.find(relativePath);
    if (it != _merged.end()) {
        return it->second;
    }
    return {};
}

void DevAssetCache::putWebP(const std::string& absolutePath,
                            std::shared_ptr<const std::vector<uint8_t>> data) const {
    if (!data || data->empty()) {
        return;
    }

    const size_t entrySize = data->size();
    std::lock_guard<std::mutex> lock(_mutex);

    if (entrySize > _maxWebpBytes) {
        std::cerr << "DevAssetCache: entry too large for cache (" << entrySize
                  << " bytes), serving without caching: " << absolutePath << std::endl;
        return;
    }

    while (!_webpOrder.empty() && _webpSizeBytes + entrySize > _maxWebpBytes) {
        const std::string& oldest = _webpOrder.front();
        const auto it = _webp.find(oldest);
        if (it != _webp.end()) {
            _webpSizeBytes -= it->second->size();
            _webp.erase(it);
        }
        _webpOrder.pop_front();
    }

    const auto existing = _webp.find(absolutePath);
    if (existing != _webp.end()) {
        _webpSizeBytes -= existing->second->size();
        _webp.erase(existing);
        for (auto orderIt = _webpOrder.begin(); orderIt != _webpOrder.end(); ++orderIt) {
            if (*orderIt == absolutePath) {
                _webpOrder.erase(orderIt);
                break;
            }
        }
    }

    _webpSizeBytes += entrySize;
    _webp[absolutePath] = std::move(data);
    _webpOrder.push_back(absolutePath);
}

bool DevAssetCache::hasWebP(const std::string& absolutePath) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _webp.find(absolutePath) != _webp.end();
}

std::shared_ptr<const std::vector<uint8_t>> DevAssetCache::getWebP(
    const std::string& absolutePath) const {
    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _webp.find(absolutePath);
    if (it != _webp.end()) {
        return it->second;
    }
    return nullptr;
}

void DevAssetCache::setMaxWebPBytes(size_t maxBytes) const {
    std::lock_guard<std::mutex> lock(_mutex);
    _maxWebpBytes = maxBytes;
}

void DevAssetCache::clear() const {
    std::lock_guard<std::mutex> lock(_mutex);
    _merged.clear();
    _webp.clear();
    _webpOrder.clear();
    _webpSizeBytes = 0;
}

}  // namespace geruest
