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

static std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            char hex[3] = {str[i + 1], str[i + 2], '\0'};
            char decodedChar = static_cast<char>(strtol(hex, nullptr, 16));
            result += decodedChar;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

class HTTPRequest {
   public:
    HTTPRequest(std::string rawRequest, std::string clientIP, std::string serverRootPath)
        : ip(std::move(clientIP)), _rawRequest(std::move(rawRequest)), serverRoot(std::move(serverRootPath)) {
        parseRequest(_rawRequest);
    }

    std::string getMethod() const { return _method; }
    std::string getPathString() const { return _path; }
    std::string getPath(size_t index) const {
        if (index < _pathParts.size()) return _pathParts[index];
        return "";
    }
    std::string getRawRequest() const { return _rawRequest; }
    std::string getRawRequestLine() const { return _rawRequestLine; }
    std::string getClientIP() const { return ip; }
    std::string getOrigin() const { return origin; }
    std::string getServerRoot() const { return serverRoot; }
    std::string getBody() const { return _body; }

    std::string getParam(const std::string& name) const {
        auto it = _queryParams.find(name);
        if (it != _queryParams.end()) return it->second;
        auto jt = _jsonParams.find(name);
        if (jt != _jsonParams.end()) return jt->second;
        auto kt = _cookies.find(name);
        if (kt != _cookies.end()) return kt->second;
        return "";
    }

    bool hasParam(const std::string& name) const {
        return _queryParams.count(name) > 0 || _jsonParams.count(name) > 0 || _cookies.count(name) > 0;
    }

    std::string getHeader(const std::string& key) const {
        auto it = _headers.find(toLower(key));
        return it != _headers.end() ? it->second : "";
    }

    bool hasHeader(const std::string& key) const { return _headers.count(toLower(key)) > 0; }

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

    void parseRequest(const std::string& rawRequest) {
        auto firstNewlinePos = rawRequest.find('\n');
        _rawRequestLine = (firstNewlinePos == std::string::npos) ? rawRequest : rawRequest.substr(0, firstNewlinePos);

        {
            std::istringstream iss(_rawRequestLine);
            iss >> _method;
            std::string pathWithQuery;
            iss >> pathWithQuery;
            parsePathAndParams(pathWithQuery);
        }

        parseHeadersAndBody(rawRequest);
    }

    void parsePathAndParams(const std::string& pathWithQuery) {
        auto questionMarkPos = pathWithQuery.find('?');
        std::string pathOnly = pathWithQuery;
        std::string queryString;

        if (questionMarkPos != std::string::npos) {
            pathOnly = pathWithQuery.substr(0, questionMarkPos);
            queryString = pathWithQuery.substr(questionMarkPos + 1);
        }

        _path = pathOnly;
        _pathParts = splitString(_path, '/');

        if (!queryString.empty()) {
            auto params = splitString(queryString, '&');
            for (const auto& p : params) {
                auto eqPos = p.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = urlDecode(p.substr(0, eqPos));
                    std::string val = urlDecode(p.substr(eqPos + 1));
                    _queryParams[key] = val;
                } else {
                    _queryParams[urlDecode(p)] = "";
                }
            }
        }
    }

    void parseHeadersAndBody(const std::string& rawRequest) {
        std::vector<std::string> delimiters = {"\r\n\r\n", "\n\n", "\r\r"};
        size_t pos = std::string::npos;

        for (const auto& delim : delimiters) {
            pos = rawRequest.find(delim);
            if (pos != std::string::npos) {
                _body = rawRequest.substr(pos + delim.length());
                parseHeaders(rawRequest.substr(0, pos));
                break;
            }
        }

        bool isJsonBody = false;
        auto it = _headers.find("content-type");
        if (it != _headers.end() && it->second.find("application/json") != std::string::npos) {
            isJsonBody = true;
        }

        if (isJsonBody && !_body.empty()) {
            parseJsonBody(_body);
        }

        auto originIt = _headers.find("origin");
        if (originIt != _headers.end()) {
            origin = originIt->second;
        }

        auto cookieIt = _headers.find("cookie");
        if (cookieIt != _headers.end()) {
            parseCookies(cookieIt->second);
        }
    }

    void parseHeaders(const std::string& headerSection) {
        std::istringstream hs(headerSection);
        std::string line;

        std::getline(hs, line);  // skip request line

        while (std::getline(hs, line)) {
            auto colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;

            std::string key = toLower(removeWhitespace(line.substr(0, colonPos)));
            std::string value = stripQuotes(trim(line.substr(colonPos + 1)));

            _headers[key] = value;
        }
    }

    void parseCookies(const std::string& cookieHeader) {
        auto cookies = splitString(cookieHeader, ';');
        for (const auto& cookie : cookies) {
            auto eqPos = cookie.find('=');
            if (eqPos != std::string::npos) {
                std::string key = trim(cookie.substr(0, eqPos));
                std::string val = trim(cookie.substr(eqPos + 1));
                _cookies[key] = val;
            }
        }
    }

    void parseJsonBody(const std::string& jsonStr) {
        std::string s = jsonStr;

        if (!s.empty() && s.front() == '{') s.erase(s.begin());
        if (!s.empty() && s.back() == '}') s.pop_back();

        auto pairs = splitString(s, ',');

        for (auto& kv : pairs) {
            auto colonPos = kv.find(':');
            if (colonPos == std::string::npos) continue;

            std::string key = stripQuotes(trim(kv.substr(0, colonPos)));
            std::string value = stripQuotes(trim(kv.substr(colonPos + 1)));

            _jsonParams[key] = value;
        }
    }

    static std::vector<std::string> splitString(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        std::string current;
        for (char c : str) {
            if (c == delimiter) {
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            result.push_back(current);
        }
        return result;
    }

    static std::string removeWhitespace(const std::string& input) {
        std::string output;
        output.reserve(input.size());
        for (char c : input) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                output.push_back(c);
            }
        }
        return output;
    }

    static std::string trim(const std::string& input) {
        size_t first = input.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = input.find_last_not_of(" \t\r\n");
        return input.substr(first, last - first + 1);
    }

    static std::string stripQuotes(const std::string& input) {
        if (input.size() >= 2) {
            char first = input.front();
            char last = input.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                return input.substr(1, input.size() - 2);
            }
        }
        return input;
    }

    static std::string toLower(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return result;
    }
};

#endif  // GERUEST_HTTPREQUEST_HPP
