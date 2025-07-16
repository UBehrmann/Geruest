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
#include <unordered_map>
#include <utility>
#include <vector>

std::string urlDecode(const std::string& str);

class HTTPRequest {
   public:
    HTTPRequest(std::string rawRequest, std::string clientIP, std::string serverRootPath);

    std::string getMethod() const;
    std::string getPathString() const;
    std::string getPath(size_t index) const;
    std::string getRawRequest() const;
    std::string getRawRequestLine() const;
    std::string getClientIP() const;
    std::string getOrigin() const;
    std::string getServerRoot() const;
    std::string getBody() const;

    std::string getParam(const std::string& name) const;

    bool hasParam(const std::string& name) const;

    std::string getHeader(const std::string& key) const;

    bool hasHeader(const std::string& key) const;

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

#endif  // GERUEST_HTTPREQUEST_HPP
