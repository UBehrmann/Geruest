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

void ServerData::addRoute(const std::string& path, RouteHandler routeHandler) {
    _routes.addRoute(path, std::move(routeHandler));
}

void ServerData::addRouteAsync(const std::string& path, AsyncRouteHandler routeHandler) {
    _routes.addRouteAsync(path, std::move(routeHandler));
}

void ServerData::addWebSocketRoute(const std::string& path, WebSocketHandler routeHandler) {
    _routes.addWebSocketRoute(path, std::move(routeHandler));
}

bool ServerData::addRedirect(const std::string& from, const std::string& to, int status) {
    return _routes.addRedirect(from, to, status);
}

size_t ServerData::addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status) {
    return _routes.addRedirects(redirects, status);
}

std::optional<std::pair<std::string, int>> ServerData::findMatchingRedirect(const std::string& path) const {
    return _routes.findMatchingRedirect(path);
}

bool ServerData::addPageGate(const std::string& path, PageGateHandler handler, const std::string& redirectTo) {
    return _gates.addPageGate(path, std::move(handler), redirectTo);
}

bool ServerData::addAsyncPageGate(const std::string& path, AsyncPageGateHandler handler,
                                  const std::string& redirectTo) {
    return _gates.addAsyncPageGate(path, std::move(handler), redirectTo);
}

bool ServerData::removePageGate(const std::string& path) { return _gates.removePageGate(path); }

void ServerData::clearPageGates() { _gates.clearPageGates(); }

std::optional<PageGateRule> ServerData::findMatchingPageGate(const std::string& path) const {
    return _gates.findMatchingPageGate(path);
}

std::optional<AsyncPageGateRule> ServerData::findMatchingAsyncPageGate(const std::string& path) const {
    return _gates.findMatchingAsyncPageGate(path);
}

std::optional<ResolvedPageGate> ServerData::findResolvedPageGate(const std::string& path) const {
    return _gates.findResolvedPageGate(path);
}

std::string ServerData::resolvePageGateRedirect(const std::string& redirectTo,
                                                const std::string& requestPath) const {
    return _gates.resolvePageGateRedirect(redirectTo, requestPath);
}

bool ServerData::addRouteGate(const std::string& path, RouteGateHandler handler) {
    return _gates.addRouteGate(path, std::move(handler));
}

bool ServerData::addAsyncRouteGate(const std::string& path, AsyncRouteGateHandler handler) {
    return _gates.addAsyncRouteGate(path, std::move(handler));
}

bool ServerData::removeRouteGate(const std::string& path) { return _gates.removeRouteGate(path); }

void ServerData::clearRouteGates() { _gates.clearRouteGates(); }

std::optional<RouteGateRule> ServerData::findMatchingRouteGate(const std::string& path) const {
    return _gates.findMatchingRouteGate(path);
}

std::optional<ResolvedRouteGate> ServerData::findResolvedRouteGate(const std::string& path) const {
    return _gates.findResolvedRouteGate(path);
}

std::optional<RouteHandler> ServerData::findMatchingRoute(const std::string& path) const {
    return _routes.findMatchingRoute(path);
}

std::optional<AsyncRouteHandler> ServerData::findMatchingAsyncRoute(const std::string& path) const {
    return _routes.findMatchingAsyncRoute(path);
}

std::optional<WebSocketHandler> ServerData::findMatchingWebSocketRoute(const std::string& path) const {
    return _routes.findMatchingWebSocketRoute(path);
}

void ServerData::setWebSocketMaxMessageBytes(size_t bytes) { _routes.setWebSocketMaxMessageBytes(bytes); }

void ServerData::setWebSocketMaxFrameBytes(size_t bytes) { _routes.setWebSocketMaxFrameBytes(bytes); }

void ServerData::setWebSocketIdleTimeout(std::chrono::seconds seconds) {
    _routes.setWebSocketIdleTimeout(seconds);
}

void ServerData::setWebSocketPingInterval(std::chrono::seconds seconds) {
    _routes.setWebSocketPingInterval(seconds);
}

void ServerData::addWebSocketSubprotocol(std::string name) { _routes.addWebSocketSubprotocol(std::move(name)); }

const WebSocketLimits& ServerData::getWebSocketLimits() const { return _routes.getWebSocketLimits(); }

const std::vector<std::string>& ServerData::getWebSocketSubprotocols() const {
    return _routes.getWebSocketSubprotocols();
}

void ServerData::setAvailableLanguages(const std::vector<std::string>& languages) {
    _languages.setAvailableLanguages(languages);
}

const std::vector<std::string>& ServerData::getAvailableLanguages() const {
    return _languages.getAvailableLanguages();
}

const std::string& ServerData::getDefaultLanguage() const { return _languages.getDefaultLanguage(); }

bool ServerData::isLanguageAvailable(const std::string& lang) const { return _languages.isLanguageAvailable(lang); }

bool ServerData::hasLanguages() const { return _languages.hasLanguages(); }

std::optional<std::string> ServerData::languagePrefixFromPath(const std::string& path) const {
    return _languages.languagePrefixFromPath(path);
}

std::string ServerData::resolvePreferredLanguage(std::string_view acceptLanguage) const {
    return _languages.resolvePreferredLanguage(acceptLanguage);
}

std::string ServerData::localizePathWithRequestLanguage(const std::string& path,
                                                        const std::string& requestPath) const {
    return _languages.localizePathWithRequestLanguage(path, requestPath);
}

void ServerData::setLogLevel(LogLevel level) { _logLevel.store(level, std::memory_order_relaxed); }

LogLevel ServerData::getLogLevel() const { return _logLevel.load(std::memory_order_relaxed); }

bool ServerData::shouldLog(LogLevel level) const {
    return static_cast<int>(level) <= static_cast<int>(_logLevel.load(std::memory_order_relaxed));
}

void ServerData::setDatabaseClient(std::shared_ptr<db::DatabaseClient> client) {
    _databaseClient = std::move(client);
}

std::shared_ptr<db::DatabaseClient> ServerData::getDatabaseClient() const { return _databaseClient; }

void ServerData::setObfuscationLevel(unsigned int level) { _obfuscation.setLevel(level); }

unsigned int ServerData::getObfuscationLevel() const { return _obfuscation.getLevel(); }

void ServerData::setObfuscationCacheExpiry(int days) { _obfuscation.setCacheExpiryDays(days); }

int ServerData::getObfuscationCacheExpiry() const { return _obfuscation.getCacheExpiryDays(); }

void ServerData::addObfuscationExclusion(const std::string& filename) { _obfuscation.addExclusion(filename); }

bool ServerData::isObfuscationExcluded(const std::string& filename) const { return _obfuscation.isExcluded(filename); }

const std::vector<std::string>& ServerData::getObfuscationExclusions() const { return _obfuscation.getExclusions(); }

void ServerData::addObfuscationPreserveIdent(const std::string& name) { _obfuscation.addPreserveIdent(name); }

void ServerData::addObfuscationExternGlobal(const std::string& name) { _obfuscation.addExternGlobal(name); }

const std::unordered_set<std::string>& ServerData::getObfuscationPreserveIdents() const {
    return _obfuscation.getPreserveIdents();
}

const std::unordered_set<std::string>& ServerData::getObfuscationExternGlobals() const {
    return _obfuscation.getExternGlobals();
}

void ServerData::loadObfuscationExternsFromText(const std::string& text) {
    _obfuscation.loadExternsFromText(text);
}

void ServerData::setObfuscationStrictUndefined(bool v) { _obfuscation.setStrictUndefined(v); }

bool ServerData::getObfuscationStrictUndefined() const { return _obfuscation.getStrictUndefined(); }

void ServerData::setObfuscationEmitGlobalThisAssignments(bool v) { _obfuscation.setEmitGlobalThisBracket(v); }

bool ServerData::getObfuscationEmitGlobalThisAssignments() const { return _obfuscation.getEmitGlobalThisBracket(); }

void ServerData::setObfuscationValidateWithAcorn(bool v) { _obfuscation.setValidateWithAcorn(v); }

bool ServerData::getObfuscationValidateWithAcorn() const { return _obfuscation.getValidateWithAcorn(); }

void ServerData::setObfuscationAutoBracketKeys(bool v) { _obfuscation.setAutoBracketKeys(v); }

bool ServerData::getObfuscationAutoBracketKeys() const { return _obfuscation.getAutoBracketKeys(); }

bool ServerData::shouldObfuscate() const { return !_devMode && _obfuscation.getLevel() > 0; }

bool ServerData::isMetricsExcludedPath(const std::string& path) {
    return ServerMetrics::isMetricsExcludedPath(path);
}

void ServerData::recordRequest() const { _metrics.recordRequest(); }

void ServerData::recordError() const { _metrics.recordError(); }

void ServerData::record4xx() const { _metrics.record4xx(); }

void ServerData::record5xx() const { _metrics.record5xx(); }

void ServerData::recordQueueRejection() const { _metrics.recordQueueRejection(); }

void ServerData::recordAcceptError() const { _metrics.recordAcceptError(); }

void ServerData::recordAcceptEmfile() const { _metrics.recordAcceptEmfile(); }

void ServerData::recordFileOpenFailure() const { _metrics.recordFileOpenFailure(); }

void ServerData::recordOverloadHttpResponse() const { _metrics.recordOverloadHttpResponse(); }

void ServerData::recordQueueFill(float fillPct) const { _metrics.recordQueueFill(fillPct); }

void ServerData::incrementActiveHandlers() const { _metrics.incrementActiveHandlers(); }

void ServerData::decrementActiveHandlers() const { _metrics.decrementActiveHandlers(); }

void ServerData::recordLatency(uint32_t us) const { _metrics.recordLatency(us); }

uint64_t ServerData::getTotalRequests() const { return _metrics.getTotalRequests(); }

uint64_t ServerData::getTotalErrors() const { return _metrics.getTotalErrors(); }

uint64_t ServerData::getTotal4xx() const { return _metrics.getTotal4xx(); }

uint64_t ServerData::getTotal5xx() const { return _metrics.getTotal5xx(); }

uint64_t ServerData::getTotalInternalErrors() const { return _metrics.getTotalInternalErrors(); }

uint64_t ServerData::getQueueRejections() const { return _metrics.getQueueRejections(); }

uint64_t ServerData::getAcceptErrorsTotal() const { return _metrics.getAcceptErrorsTotal(); }

uint64_t ServerData::getAcceptEmfileTotal() const { return _metrics.getAcceptEmfileTotal(); }

uint64_t ServerData::getFileOpenFailures() const { return _metrics.getFileOpenFailures(); }

uint64_t ServerData::getOverloadHttpResponses() const { return _metrics.getOverloadHttpResponses(); }

int64_t ServerData::getActiveHandlers() const { return _metrics.getActiveHandlers(); }

ServerData::WindowMetrics ServerData::getWindowMetricsHour() const { return _metrics.getWindowMetricsHour(); }

ServerData::WindowMetrics ServerData::getRollingAveragePerHour() const {
    return _metrics.getRollingAveragePerHour();
}

ServerData::LatencyStats ServerData::getLatencyStats(uint32_t windowSeconds) const {
    return _metrics.getLatencyStats(windowSeconds);
}

uint64_t ServerData::getUptimeSeconds() const { return _metrics.getUptimeSeconds(); }

bool ServerData::loadPersistentMetricsFromFile(const std::string& path) {
    return _metrics.loadPersistentMetricsFromFile(path);
}

bool ServerData::savePersistentMetricsToFile(const std::string& path) const {
    return _metrics.savePersistentMetricsToFile(path);
}

double ServerData::getUptimeHoursTotal() const { return _metrics.getUptimeHoursTotal(); }

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
