/**
 * @file StaticFileResolver.hpp
 * @brief URL path to filesystem path resolution for static assets.
 */

#ifndef GERUEST_STATICFILERESOLVER_HPP
#define GERUEST_STATICFILERESOLVER_HPP

#include <functional>
#include <string>

#include "data/HTTPRequest.hpp"
#include "data/ServerData.hpp"

namespace geruest {

class StaticFileResolver {
   public:
    explicit StaticFileResolver(const ServerData& serverData,
                                std::function<void(const std::string&)> logError = {});

    static std::string getExtension(const std::string& path);
    static std::string getContentType(const std::string& extension);

    /** Mutates pathReceived (existing contract). Returns empty on blocked/unknown paths. */
    std::string buildPath(std::string& pathReceived, const std::string& extension,
                          const HTTPRequest& request) const;

   private:
    const ServerData& serverData_;
    std::function<void(const std::string&)> logError_;
};

}  // namespace geruest

#endif  // GERUEST_STATICFILERESOLVER_HPP
