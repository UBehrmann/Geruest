/**
 * @file ServerData.cpp
 * @brief ServerData facade helpers and merged-asset owner lookup.
 */

#include "ServerData.hpp"

#include "builders/AssetMerger.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace geruest {

void ServerData::wireLanguagePointers_() {
    _routes.setLanguageConfig(&_languages);
    _gates.setLanguageConfig(&_languages);
}

ServerData::ServerData() {
    wireLanguagePointers_();
}

ServerData::ServerData(const ServerData& other)
    : _languages(other._languages),
      _routes(other._routes),
      _gates(other._gates),
      _obfuscation(other._obfuscation),
      _root(other._root),
      _removeComments(other._removeComments),
      _mergeAssets(other._mergeAssets),
      _devMode(other._devMode),
      _webpConversion(other._webpConversion),
      _webpQuality(other._webpQuality),
      _maxRequestsPerConnection(other._maxRequestsPerConnection),
      _textResponseCacheMaxEntryBytes(other._textResponseCacheMaxEntryBytes),
      _textResponseCacheMaxTotalBytes(other._textResponseCacheMaxTotalBytes),
      _notFoundPage(other._notFoundPage),
      _basicAuth(other._basicAuth),
      _logLevel(other._logLevel.load(std::memory_order_relaxed)),
      _databaseClient(other._databaseClient) {
    wireLanguagePointers_();
}

ServerData& ServerData::operator=(const ServerData& other) {
    if (this != &other) {
        _languages = other._languages;
        _routes = other._routes;
        _gates = other._gates;
        _obfuscation = other._obfuscation;
        _root = other._root;
        _removeComments = other._removeComments;
        _mergeAssets = other._mergeAssets;
        _devMode = other._devMode;
        _webpConversion = other._webpConversion;
        _webpQuality = other._webpQuality;
        _maxRequestsPerConnection = other._maxRequestsPerConnection;
        _textResponseCacheMaxEntryBytes = other._textResponseCacheMaxEntryBytes;
        _textResponseCacheMaxTotalBytes = other._textResponseCacheMaxTotalBytes;
        _notFoundPage = other._notFoundPage;
        _basicAuth = other._basicAuth;
        _logLevel.store(other._logLevel.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _databaseClient = other._databaseClient;
        wireLanguagePointers_();
    }
    return *this;
}

ServerData::ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string root) : _root(std::move(root)) {
    for (const auto& entry : routes) {
        _routes.addRoute(entry.first, entry.second);
    }
    wireLanguagePointers_();
}

std::unordered_map<std::string, RouteHandler> ServerData::getRoutes() {
    return _routes.getRoutesMerged();
}

const std::unordered_map<std::string, RouteHandler>& ServerData::getRoutes() const {
    return _routes.getExactRoutes();
}

void ServerData::setWebPQuality(float quality) {
    _webpQuality = quality;
    if (_webpQuality < 0.0f) {
        _webpQuality = 0.0f;
    }
    if (_webpQuality > 100.0f) {
        _webpQuality = 100.0f;
    }
}

void ServerData::enableDevMode() {
    _devMode = true;
    setLogLevel(LogLevel::Debug);
    _removeComments = false;
    _textResponseCacheMaxEntryBytes = 0;
    _textResponseCacheMaxTotalBytes = 0;
}

bool ServerData::pageRequiresAccessControl(const std::string& pagePath) const {
    const std::string canon = canonicalRequestPath(pagePath);
    return findResolvedPageGate(canon).has_value() || _basicAuth.requiresAuth(canon);
}

std::optional<std::string> ServerData::findMergedAssetOwnerPagePath(const std::string& assetRequestPath) const {
    if (!_mergeAssets || _root.empty()) {
        return std::nullopt;
    }

    const std::string canon = canonicalRequestPath(assetRequestPath);
    const bool isJs = canon.size() > 3 && canon.compare(canon.size() - 3, 3, ".js") == 0;
    const bool isCss = canon.size() > 4 && canon.compare(canon.size() - 4, 4, ".css") == 0;
    if (!isJs && !isCss) {
        return std::nullopt;
    }

    const size_t dotPos = canon.find_last_of('.');
    if (dotPos == std::string::npos || dotPos <= 1) {
        return std::nullopt;
    }

    const size_t lastSlash = canon.find_last_of('/');
    const std::string pageStem = (lastSlash == std::string::npos) ? canon.substr(1, dotPos - 1)
                                                                  : canon.substr(lastSlash + 1, dotPos - lastSlash - 1);
    if (pageStem.empty()) {
        return std::nullopt;
    }

    const std::string htmlRoot = _root + "/html";
    if (!std::filesystem::is_directory(htmlRoot)) {
        return std::nullopt;
    }

    const std::string templatePath = AssetMerger::findHtmlTemplateByPageName(htmlRoot, pageStem);
    if (templatePath.empty()) {
        return std::nullopt;
    }

    std::ifstream in(templatePath, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::string htmlContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (htmlContent.empty()) {
        return std::nullopt;
    }

    AssetMerger merger(_root, _removeComments, _obfuscation.getExclusions());
    const std::string pageName = AssetMerger::pageNameFromHtmlPath(templatePath);
    const std::vector<std::string> predicted = merger.predictMergedAssetUrls(htmlContent, pageName);
    for (const std::string& url : predicted) {
        if (canonicalRequestPath(url) == canon) {
            return AssetMerger::sitePathFromHtmlFile(htmlRoot, templatePath);
        }
    }
    return std::nullopt;
}

}  // namespace geruest
