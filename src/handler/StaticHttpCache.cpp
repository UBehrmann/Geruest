/**
 * @file StaticHttpCache.cpp
 */

#include "StaticHttpCache.hpp"

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"
#include "data/SiteRevision.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <string_view>

namespace geruest {
namespace {

bool iequalsAscii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string_view trimAscii(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::string_view stripWeakEtagPrefix(std::string_view tag) {
    tag = trimAscii(tag);
    if (tag.size() >= 2 && tag[0] == 'W' && tag[1] == '/') {
        tag.remove_prefix(2);
        tag = trimAscii(tag);
    }
    if (tag.size() >= 2 && tag.front() == '"' && tag.back() == '"') {
        tag = tag.substr(1, tag.size() - 2);
    }
    return tag;
}

bool etagValuesMatch(std::string_view left, std::string_view right) {
    return iequalsAscii(stripWeakEtagPrefix(left), stripWeakEtagPrefix(right));
}

bool isHtmlContentType(std::string_view contentType) {
    const size_t semi = contentType.find(';');
    const std::string_view base = trimAscii(contentType.substr(0, semi));
    return iequalsAscii(base, "text/html");
}

std::string makeCacheControlPublicMaxAge(int seconds, bool immutable) {
    std::string value = "public, max-age=" + std::to_string(seconds);
    if (immutable) {
        value += ", immutable";
    }
    return value;
}

}  // namespace

std::string formatHttpDate(std::filesystem::file_time_type fileTime) {
    const auto sysTime = std::filesystem::file_time_type::clock::to_sys(fileTime);
    const std::time_t epoch = std::chrono::system_clock::to_time_t(sysTime);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &epoch);
#else
    gmtime_r(&epoch, &utc);
#endif

    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &utc) == 0) {
        return {};
    }
    return buffer;
}

std::string makeEtagFromBody(std::string_view body) {
    return "\"" + fnv1a64Hex(body) + "\"";
}

std::string makeEtagFromFile(const std::string& contentPath, std::uintmax_t fileSize) {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(contentPath, ec);
    if (ec) {
        return makeEtagFromBody(contentPath + ":" + std::to_string(fileSize));
    }

    std::string material;
    material.reserve(contentPath.size() + 32);
    material.append(contentPath);
    material.push_back(':');
    material.append(reinterpret_cast<const char*>(&mtime), sizeof(mtime));
    material.push_back(':');
    material.append(std::to_string(fileSize));
    return "\"" + fnv1a64Hex(material) + "\"";
}

StaticCacheHeaders resolveStaticCacheHeaders(const ServerData& serverData, std::string_view contentType,
                                             std::string_view /*extension*/, bool perRequestPrivate) {
    StaticCacheHeaders headers;

    if (serverData.isDevMode()) {
        headers.cacheControl = "no-store";
        return headers;
    }

    if (perRequestPrivate) {
        headers.cacheControl = "private, no-cache";
        return headers;
    }

    if (isHtmlContentType(contentType)) {
        const int htmlMaxAge = serverData.getStaticHtmlCacheMaxAge();
        if (htmlMaxAge <= 0) {
            headers.cacheControl = "public, max-age=0, must-revalidate";
        } else {
            headers.cacheControl = makeCacheControlPublicMaxAge(htmlMaxAge, false);
        }
        return headers;
    }

    const int assetMaxAge = serverData.getStaticCacheMaxAge();
    headers.cacheControl = makeCacheControlPublicMaxAge(assetMaxAge, true);
    return headers;
}

bool matchesNotModified(const HTTPRequest& request, const StaticCacheHeaders& headers) {
    if (headers.etag.empty() && headers.lastModified.empty()) {
        return false;
    }

    const std::string_view ifNoneMatch = request.getHeaderView("if-none-match");
    if (!ifNoneMatch.empty()) {
        if (ifNoneMatch == "*") {
            return !headers.etag.empty();
        }

        std::string_view list = ifNoneMatch;
        while (!list.empty()) {
            const size_t comma = list.find(',');
            const std::string_view token = trimAscii(list.substr(0, comma));
            if (!token.empty() && !headers.etag.empty() && etagValuesMatch(token, headers.etag)) {
                return true;
            }
            if (comma == std::string_view::npos) {
                break;
            }
            list.remove_prefix(comma + 1);
        }
    }

    if (!headers.lastModified.empty()) {
        const std::string_view ifModifiedSince = trimAscii(request.getHeaderView("if-modified-since"));
        if (!ifModifiedSince.empty() && iequalsAscii(ifModifiedSince, headers.lastModified)) {
            return true;
        }
    }

    return false;
}

void applyCacheHeaders(HTTPResponse& response, const StaticCacheHeaders& headers) {
    if (!headers.cacheControl.empty()) {
        response.setHeader("Cache-Control", headers.cacheControl);
    }
    if (!headers.etag.empty()) {
        response.setHeader("ETag", headers.etag);
    }
    if (!headers.lastModified.empty()) {
        response.setHeader("Last-Modified", headers.lastModified);
    }
}

}  // namespace geruest
