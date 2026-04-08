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
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace geruest {

std::string urlDecode(const std::string& str);

class HTTPRequest {
   public:
    HTTPRequest(std::string rawRequest, std::string clientIP, std::string serverRootPath);

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

    bool hasHeader(std::string_view key) const;

   private:
    std::string ip;
    std::string origin;
    std::string serverRoot;

    std::string _method;
    std::string _path;
    std::string _rawRequestLine;
    std::string _body;

    std::string _rawRequest;

    std::vector<std::string> _pathParts;
    std::unordered_map<std::string, std::string> _queryParams;
    std::unordered_map<std::string, std::string> _jsonParams;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _cookies;

    void parseRequest(const std::string& rawRequest);

    void parsePathAndParams(const std::string& pathWithQuery);

    void parseHeadersAndBody(const std::string& rawRequest);

    void parseHeaders(const std::string& headerSection);

    void parseCookies(const std::string& cookieHeader);

    void parseJsonBody(const std::string& jsonStr);

    static std::vector<std::string> splitString(const std::string& str, char delimiter);

    static std::string removeWhitespace(const std::string& input);

    static std::string trim(const std::string& input);

    static std::string stripQuotes(const std::string& input);

    static std::string toLower(const std::string& str);
};

/** True when an Expect header value requests 100-continue (trimmed, case-insensitive, RFC 7231). */
[[nodiscard]] inline bool httpExpectIs100Continue(const std::string& value) {
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
        if (std::tolower(static_cast<unsigned char>(value[i + k])) !=
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
