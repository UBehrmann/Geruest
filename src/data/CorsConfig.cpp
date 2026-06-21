/**
 * @file CorsConfig.cpp
 */

#include "CorsConfig.hpp"

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "WildcardMatch.hpp"

#include <algorithm>

namespace geruest {

namespace {

constexpr const char* kDefaultAllowHeaders = "Content-Type, Authorization";
constexpr const char* kAllowMethods = "GET, POST, PUT, DELETE, PATCH, OPTIONS";

bool originMatchesEntry(std::string_view origin, const std::string& allowed) {
    return origin == allowed;
}

}  // namespace

CorsConfig CorsConfig::fromOptions(const CorsOptions& options) {
    CorsConfig config;
    if (options.origins.empty() || options.paths.empty()) {
        return config;
    }
    config._enabled = true;
    config._origins = options.origins;
    config._paths = options.paths;
    return config;
}

bool CorsConfig::matchesPath(const std::string& path) const {
    if (!_enabled) {
        return false;
    }
    for (const std::string& pattern : _paths) {
        if (matchesWildcardPattern(pattern, path)) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> CorsConfig::resolveOrigin(std::string_view requestOrigin) const {
    if (!_enabled || requestOrigin.empty()) {
        return std::nullopt;
    }

    for (const std::string& allowed : _origins) {
        if (allowed == "*") {
            return std::string("*");
        }
        if (originMatchesEntry(requestOrigin, allowed)) {
            return std::string(requestOrigin);
        }
    }
    return std::nullopt;
}

void applyCorsHeaders(HTTPResponse& response, const CorsConfig& cors, const HTTPRequest* request,
                      bool preflight) {
    if (!cors.isEnabled() || request == nullptr) {
        return;
    }

    const std::string& path = request->getPathString();
    if (!cors.matchesPath(path)) {
        return;
    }

    const std::optional<std::string> allowedOrigin = cors.resolveOrigin(request->getHeaderView("origin"));
    if (!allowedOrigin.has_value()) {
        return;
    }

    response.setHeader("Access-Control-Allow-Origin", *allowedOrigin);
    response.setHeader("Access-Control-Allow-Methods", kAllowMethods);
    response.setHeader("Vary", "Origin");

    if (preflight) {
        const std::string_view requestedHeaders = request->getHeaderView("access-control-request-headers");
        if (!requestedHeaders.empty()) {
            response.setHeader("Access-Control-Allow-Headers", std::string(requestedHeaders));
        } else {
            response.setHeader("Access-Control-Allow-Headers", kDefaultAllowHeaders);
        }
        response.setHeader("Access-Control-Max-Age", "86400");
    } else {
        response.setHeader("Access-Control-Allow-Headers", kDefaultAllowHeaders);
    }
}

}  // namespace geruest
