/**
 * @file CorsConfig.hpp
 * @brief Path-scoped CORS allowlist and preflight header helpers.
 */

#ifndef GERUEST_CORSCONFIG_HPP
#define GERUEST_CORSCONFIG_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace geruest {

class HTTPRequest;
class HTTPResponse;

/** Options for Geruest::enableCors(). */
struct CorsOptions {
    /** Allowed Origin values, or "*" for any origin. */
    std::vector<std::string> origins;
    /** Request paths to protect (exact or wildcard, e.g. "/v1/..."). */
    std::vector<std::string> paths;
    /**
     * Allowed request headers for preflight (case-insensitive).
     * Empty uses the built-in default set (Content-Type, Authorization, etc.).
     */
    std::vector<std::string> allowHeaders;
};

class CorsConfig {
   public:
    CorsConfig() = default;

    static CorsConfig fromOptions(const CorsOptions& options);

    bool isEnabled() const { return _enabled; }
    const std::vector<std::string>& origins() const { return _origins; }
    const std::vector<std::string>& paths() const { return _paths; }

    bool matchesPath(const std::string& path) const;
    std::optional<std::string> resolveOrigin(std::string_view requestOrigin) const;
    const std::vector<std::string>& allowHeaders() const { return _allowHeaders; }

   private:
    bool _enabled = false;
    std::vector<std::string> _origins;
    std::vector<std::string> _paths;
    std::vector<std::string> _allowHeaders;
};

/** Add CORS headers when config matches path + origin. No-op when disabled. */
void applyCorsHeaders(HTTPResponse& response, const CorsConfig& cors, const HTTPRequest* request,
                      bool preflight = false);

}  // namespace geruest

#endif  // GERUEST_CORSCONFIG_HPP
