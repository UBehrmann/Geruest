/**
 * @file HTTPRequest.hpp
 * @date 19.03.2025
 *
 * @author Urs Behrmann
 *
 * @brief Parse the headers of a HTTP request
 */

#ifndef GERUEST_HTTPREQUEST_HPP
#define GERUEST_HTTPREQUEST_HPP

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "database/DatabaseClient.hpp"

namespace geruest {

std::string urlDecode(std::string_view str);
inline std::string urlDecode(const std::string& str) { return urlDecode(std::string_view(str)); }

/** Tag: parse only headers + request line from a prefix view (caller must keep storage alive for ctor duration). */
struct HttpHeadersOnlyTag {
    explicit HttpHeadersOnlyTag() = default;
};

struct HeaderMapHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept;
    [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept { return (*this)(std::string_view(s)); }
};

struct HeaderMapEq {
    using is_transparent = void;
    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const noexcept;
    [[nodiscard]] bool operator()(std::string_view a, const std::string& b) const noexcept {
        return (*this)(a, std::string_view(b));
    }
    [[nodiscard]] bool operator()(const std::string& a, std::string_view b) const noexcept {
        return (*this)(std::string_view(a), b);
    }
    [[nodiscard]] bool operator()(const std::string& a, const std::string& b) const noexcept {
        return (*this)(std::string_view(a), std::string_view(b));
    }
};

using HeaderFieldMap = std::unordered_map<std::string, std::string, HeaderMapHash, HeaderMapEq>;

class HTTPRequest {
   public:
    HTTPRequest(std::string rawRequest, std::string clientIP, std::string serverRootPath,
                std::shared_ptr<db::DatabaseClient> databaseClient = nullptr);

    /** Full message bytes owned by `backing` (typically one per request from Handler). */
    HTTPRequest(std::shared_ptr<const std::string> backing, std::string clientIP, std::string serverRootPath,
                std::shared_ptr<db::DatabaseClient> databaseClient = nullptr);

    /** Prefix through end of headers (see Handler `headerEnd`); no body; synchronous parse only. */
    HTTPRequest(HttpHeadersOnlyTag, std::string_view headerBytesPrefix, std::string clientIP,
                std::string serverRootPath, std::shared_ptr<db::DatabaseClient> databaseClient = nullptr);

    const std::string& getMethod() const;
    const std::string& getPathString() const;
    std::string getPath(size_t index) const;
    const std::string& getRawRequest() const;
    const std::string& getRawRequestLine() const;
    const std::string& getClientIP() const;
    const std::string& getOrigin() const;
    const std::string& getServerRoot() const;
    const std::string& getBody() const;

    std::string getParam(const std::string& name) const;

    bool hasParam(const std::string& name) const;

    std::string getHeader(std::string_view key) const;
    std::string_view getHeaderView(std::string_view key) const;

    bool hasHeader(std::string_view key) const;
    std::shared_ptr<db::DatabaseClient> database() const;

   private:
    std::string ip;
    std::string origin;
    std::string serverRoot;

    std::string _method;
    std::string _path;
    std::string _rawRequestLine;

    std::shared_ptr<const std::string> _backing;
    std::string_view _bodyView;
    mutable std::optional<std::string> _bodyMaterialized;

    std::vector<std::string> _pathParts;
    std::unordered_map<std::string, std::string> _queryParams;
    std::unordered_map<std::string, std::string> _jsonParams;
    HeaderFieldMap _headers;
    std::unordered_map<std::string, std::string> _cookies;
    std::shared_ptr<db::DatabaseClient> _databaseClient;

    bool _headersOnly = false;

    mutable std::optional<std::string> _rawRequestMaterialized;

    void parseFromWire(std::string_view raw);
    void parsePathAndParams(std::string_view pathWithQuery);
    void parseHeadersAndBody(std::string_view rawRequest);
    void parseHeaders(std::string_view headerSection);
    void parseCookies(std::string_view cookieHeader);
    void parseJsonBody();

    static std::vector<std::string> splitPathSegments(std::string_view pathOnly);
    static std::string removeWhitespace(std::string_view input);
    static std::string trim(std::string_view input);
    static std::string stripQuotes(std::string_view input);
    static std::string toLower(std::string_view str);
};

[[nodiscard]] inline bool httpExpectIs100Continue(std::string_view value) {
    size_t i = 0;
    while (i < value.size() && std::isspace(static_cast<unsigned char>(value[i]))) {
        ++i;
    }
    size_t j = value.size();
    while (j > i && std::isspace(static_cast<unsigned char>(value[j - 1]))) {
        --j;
    }
    static const char kExpected[] = "100-continue";
    constexpr size_t kLen = sizeof(kExpected) - 1;
    if (j - i < kLen) {
        return false;
    }
    for (size_t k = 0; k < kLen; ++k) {
        const unsigned char c = static_cast<unsigned char>(value[i + k]);
        const unsigned char lower = (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
        if (lower !=
            static_cast<unsigned char>(kExpected[k])) {
            return false;
        }
    }
    if (j - i > kLen) {
        const unsigned char next = static_cast<unsigned char>(value[i + kLen]);
        if (next != ',' && !std::isspace(next)) {
            return false;
        }
    }
    return true;
}

}  // namespace geruest

#endif  // GERUEST_HTTPREQUEST_HPP
