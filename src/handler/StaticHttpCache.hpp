/**
 * @file StaticHttpCache.hpp
 * @brief Browser cache validators and Cache-Control policy for static responses.
 */

#ifndef GERUEST_STATICHTTPCACHE_HPP
#define GERUEST_STATICHTTPCACHE_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace geruest {

class HTTPRequest;
class HTTPResponse;
class ServerData;

struct StaticCacheHeaders {
    std::string cacheControl;
    std::string etag;
    std::string lastModified;
};

std::string formatHttpDate(std::filesystem::file_time_type fileTime);

std::string makeEtagFromBody(std::string_view body);

std::string makeEtagFromFile(const std::string& contentPath, std::uintmax_t fileSize);

StaticCacheHeaders resolveStaticCacheHeaders(const ServerData& serverData, std::string_view contentType,
                                             std::string_view extension, bool perRequestPrivate);

bool matchesNotModified(const HTTPRequest& request, const StaticCacheHeaders& headers);

void applyCacheHeaders(HTTPResponse& response, const StaticCacheHeaders& headers);

}  // namespace geruest

#endif  // GERUEST_STATICHTTPCACHE_HPP
