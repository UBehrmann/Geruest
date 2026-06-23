/**
 * @file CorsConfig.cpp
 */

#include "CorsConfig.hpp"

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "WildcardMatch.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace geruest {

namespace {

constexpr const char* kDefaultAllowHeaders = "Content-Type, Authorization, Accept, X-Requested-With";
constexpr const char* kAllowMethods = "GET, POST, PUT, DELETE, PATCH, OPTIONS";

const std::vector<std::string> kBuiltInAllowHeaders = {
    "content-type", "authorization", "accept", "x-requested-with",
};

bool originMatchesEntry(std::string_view origin, const std::string& allowed) {
    return origin == allowed;
}

std::string headerToLower(std::string_view header) {
    std::string out;
    out.reserve(header.size());
    for (unsigned char c : header) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string trimHeaderToken(std::string_view token) {
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) {
        token.remove_prefix(1);
    }
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) {
        token.remove_suffix(1);
    }
    return std::string(token);
}

std::vector<std::string> normalizeAllowHeaderList(const std::vector<std::string>& headers) {
    std::vector<std::string> normalized;
    normalized.reserve(headers.size());
    for (const std::string& header : headers) {
        const std::string trimmed = trimHeaderToken(header);
        if (!trimmed.empty()) {
            normalized.push_back(headerToLower(trimmed));
        }
    }
    return normalized;
}

std::string filterRequestedHeaders(std::string_view requested, const std::vector<std::string>& allowHeaders) {
    std::unordered_set<std::string> allowed(allowHeaders.begin(), allowHeaders.end());
    std::vector<std::string> matched;
    matched.reserve(4);

    while (!requested.empty()) {
        const std::size_t comma = requested.find(',');
        const std::string token = trimHeaderToken(requested.substr(0, comma));
        if (comma == std::string_view::npos) {
            requested = {};
        } else {
            requested.remove_prefix(comma + 1);
        }
        if (token.empty()) {
            continue;
        }
        const std::string lower = headerToLower(token);
        if (allowed.find(lower) != allowed.end()) {
            matched.push_back(token);
        }
    }

    if (matched.empty()) {
        return std::string(kDefaultAllowHeaders);
    }

    std::ostringstream joined;
    for (std::size_t i = 0; i < matched.size(); ++i) {
        if (i > 0) {
            joined << ", ";
        }
        joined << matched[i];
    }
    return joined.str();
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
    if (options.allowHeaders.empty()) {
        config._allowHeaders = kBuiltInAllowHeaders;
    } else {
        config._allowHeaders = normalizeAllowHeaderList(options.allowHeaders);
    }
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
            response.setHeader("Access-Control-Allow-Headers",
                               filterRequestedHeaders(requestedHeaders, cors.allowHeaders()));
        } else {
            response.setHeader("Access-Control-Allow-Headers", kDefaultAllowHeaders);
        }
        response.setHeader("Access-Control-Max-Age", "86400");
    } else {
        response.setHeader("Access-Control-Allow-Headers", kDefaultAllowHeaders);
    }
}

}  // namespace geruest
