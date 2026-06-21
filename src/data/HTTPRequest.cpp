/**
 * @file HTTPRequest.cpp
 * @date 16.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Parse the headers of a HTTP request
 */

#include "HTTPRequest.hpp"

#include <charconv>
#include <cstdlib>
#include <string>

#include "parser/JSONParser.hpp"

namespace {

constexpr std::size_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::size_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] inline unsigned char asciiLower(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] std::size_t hashLowerFnv1a(std::string_view sv) noexcept {
    std::size_t h = kFnvOffsetBasis;
    for (unsigned char c : sv) {
        h ^= static_cast<std::size_t>(asciiLower(c));
        h *= kFnvPrime;
    }
    return h;
}

[[nodiscard]] bool iequalsAscii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (asciiLower(static_cast<unsigned char>(a[i])) != asciiLower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string_view trimSv(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

[[nodiscard]] std::string_view stripQuotesSv(std::string_view s) {
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s.remove_prefix(1);
            s.remove_suffix(1);
        }
    }
    return s;
}

[[nodiscard]] std::string_view firstLine(std::string_view block) {
    const std::size_t n = block.find('\n');
    if (n == std::string_view::npos) {
        return block;
    }
    std::string_view line = block.substr(0, n);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

[[nodiscard]] std::string_view advancePastFirstLine(std::string_view block) {
    const std::size_t n = block.find('\n');
    if (n == std::string_view::npos) {
        return {};
    }
    return block.substr(n + 1);
}

/** Lowercase + drop ASCII whitespace into `out` (reused across header lines). */
void appendLowerNoWhitespace(std::string& out, std::string_view input) {
    out.clear();
    out.reserve(input.size());
    for (unsigned char c : input) {
        if (!std::isspace(c)) {
            out.push_back(static_cast<char>(asciiLower(c)));
        }
    }
}

[[nodiscard]] bool iequalsAsciiNoSpace(std::string_view a, std::string_view b) {
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size()) {
        while (i < a.size() && std::isspace(static_cast<unsigned char>(a[i]))) {
            ++i;
        }
        while (j < b.size() && std::isspace(static_cast<unsigned char>(b[j]))) {
            ++j;
        }
        if (i >= a.size() || j >= b.size()) {
            break;
        }
        if (asciiLower(static_cast<unsigned char>(a[i])) != asciiLower(static_cast<unsigned char>(b[j]))) {
            return false;
        }
        ++i;
        ++j;
    }
    while (i < a.size() && std::isspace(static_cast<unsigned char>(a[i]))) {
        ++i;
    }
    while (j < b.size() && std::isspace(static_cast<unsigned char>(b[j]))) {
        ++j;
    }
    return i == a.size() && j == b.size();
}

}  // namespace

namespace geruest {

std::optional<HttpHeaderSplit> splitHttpHeaders(std::string_view raw) {
    static constexpr std::string_view kDelim0 = "\r\n\r\n";
    static constexpr std::string_view kDelim1 = "\n\n";
    static constexpr std::string_view kDelim2 = "\r\r";

    std::size_t pos = std::string_view::npos;
    std::size_t delimLen = 0;

    pos = raw.find(kDelim0);
    if (pos != std::string_view::npos) {
        delimLen = kDelim0.size();
    } else {
        pos = raw.find(kDelim1);
        if (pos != std::string_view::npos) {
            delimLen = kDelim1.size();
        } else {
            pos = raw.find(kDelim2);
            if (pos != std::string_view::npos) {
                delimLen = kDelim2.size();
            }
        }
    }

    if (pos == std::string_view::npos) {
        return std::nullopt;
    }

    return HttpHeaderSplit{pos + delimLen, delimLen};
}

HeaderPreflight parseHeaderPreflight(std::string_view headerPrefix) {
    HeaderPreflight out{};
    const size_t firstNl = headerPrefix.find('\n');
    if (firstNl == std::string_view::npos) {
        return out;
    }
    std::string_view rest = headerPrefix.substr(firstNl + 1);
    while (!rest.empty()) {
        const size_t lineEnd = rest.find('\n');
        std::string_view line = rest.substr(0, lineEnd);
        if (lineEnd == std::string_view::npos) {
            rest = {};
        } else {
            rest.remove_prefix(lineEnd + 1);
        }
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        const std::string_view key = line.substr(0, colon);
        const std::string_view value = trimSv(line.substr(colon + 1));
        if (out.expect.empty() && iequalsAsciiNoSpace(key, "expect")) {
            out.expect = value;
        } else if (out.contentLength.empty() && iequalsAsciiNoSpace(key, "content-length")) {
            out.contentLength = value;
        } else if (out.transferEncoding.empty() && iequalsAsciiNoSpace(key, "transfer-encoding")) {
            out.transferEncoding = value;
        }
        if (!out.expect.empty() && !out.contentLength.empty() && !out.transferEncoding.empty()) {
            break;
        }
    }
    return out;
}

size_t findChunkedBodyEnd(const std::string& raw, size_t bodyStart) {
    size_t pos = bodyStart;
    while (true) {
        const size_t lineEnd = raw.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            return std::string::npos;
        }

        std::string_view sizeLine(raw.data() + pos, lineEnd - pos);
        const size_t semi = sizeLine.find(';');
        if (semi != std::string_view::npos) {
            sizeLine = sizeLine.substr(0, semi);
        }
        sizeLine = trimSv(sizeLine);
        if (sizeLine.empty()) {
            return std::string::npos;
        }

        unsigned long long chunkSize = 0;
        const char* begin = sizeLine.data();
        const char* end = sizeLine.data() + sizeLine.size();
        const std::from_chars_result parsed = std::from_chars(begin, end, chunkSize, 16);
        if (parsed.ec != std::errc() || parsed.ptr != end) {
            return std::string::npos;
        }

        pos = lineEnd + 2;
        if (chunkSize == 0) {
            if (pos + 2 <= raw.size() && raw.compare(pos, 2, "\r\n") == 0) {
                return pos + 2;
            }
            const size_t trailerEnd = raw.find("\r\n\r\n", pos);
            if (trailerEnd == std::string::npos) {
                return std::string::npos;
            }
            return trailerEnd + 4;
        }

        const size_t chunkDataEnd = pos + static_cast<size_t>(chunkSize);
        if (chunkDataEnd + 2 > raw.size()) {
            return std::string::npos;
        }
        if (raw.compare(chunkDataEnd, 2, "\r\n") != 0) {
            return std::string::npos;
        }
        pos = chunkDataEnd + 2;
    }
}

bool parseContentLengthBytes(std::string_view cl, size_t* out) {
    if (cl.empty()) {
        return false;
    }
    unsigned long long v = 0;
    const char* begin = cl.data();
    const char* end = cl.data() + cl.size();
    const std::from_chars_result result = std::from_chars(begin, end, v);
    if (result.ec != std::errc() || result.ptr != end) {
        return false;
    }
    *out = static_cast<size_t>(v);
    return true;
}

std::size_t HeaderMapHash::operator()(std::string_view sv) const noexcept { return hashLowerFnv1a(sv); }

bool HeaderMapEq::operator()(std::string_view a, std::string_view b) const noexcept { return iequalsAscii(a, b); }

std::string urlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i) {
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

HTTPRequest::HTTPRequest(std::string rawRequest, std::string clientIP, std::string serverRootPath,
                         std::shared_ptr<db::DatabaseClient> databaseClient)
    : ip(std::move(clientIP)),
      serverRoot(std::move(serverRootPath)),
      _backing(std::make_shared<const std::string>(std::move(rawRequest))),
      _databaseClient(std::move(databaseClient)) {
    parseFromWire(std::string_view(_backing->data(), _backing->size()));
}

HTTPRequest::HTTPRequest(std::shared_ptr<const std::string> backing, std::string clientIP,
                         std::string serverRootPath, std::shared_ptr<db::DatabaseClient> databaseClient)
    : ip(std::move(clientIP)),
      serverRoot(std::move(serverRootPath)),
      _backing(std::move(backing)),
      _databaseClient(std::move(databaseClient)) {
    parseFromWire(std::string_view(_backing->data(), _backing->size()));
}

HTTPRequest::HTTPRequest(HttpHeadersOnlyTag /*tag*/, std::string_view headerBytesPrefix, std::string clientIP,
                        std::string serverRootPath, std::shared_ptr<db::DatabaseClient> databaseClient)
    : ip(std::move(clientIP)),
      serverRoot(std::move(serverRootPath)),
      _databaseClient(std::move(databaseClient)),
      _headersOnly(true) {
    parseFromWire(headerBytesPrefix);
}

const std::string& HTTPRequest::getMethod() const { return _method; }
const std::string& HTTPRequest::getPathString() const { return _path; }
std::string HTTPRequest::getPath(size_t index) const {
    if (index < _pathParts.size()) {
        return _pathParts[index];
    }
    return "";
}

const std::string& HTTPRequest::getRawRequest() const {
    static const std::string kEmpty;
    if (_headersOnly || !_backing) {
        return kEmpty;
    }
    if (!_rawRequestMaterialized) {
        _rawRequestMaterialized.emplace(*_backing);
    }
    return *_rawRequestMaterialized;
}

const std::string& HTTPRequest::getRawRequestLine() const { return _rawRequestLine; }
const std::string& HTTPRequest::getClientIP() const { return ip; }
const std::string& HTTPRequest::getOrigin() const { return origin; }
const std::string& HTTPRequest::getServerRoot() const { return serverRoot; }

const std::string& HTTPRequest::getBody() const {
    static const std::string kEmpty;
    if (_bodyView.empty()) {
        return kEmpty;
    }
    if (!_bodyMaterialized) {
        _bodyMaterialized.emplace(_bodyView);
    }
    return *_bodyMaterialized;
}

std::string HTTPRequest::getParam(const std::string& name) const {
    auto it = _queryParams.find(name);
    if (it != _queryParams.end()) {
        return it->second;
    }
    auto jt = _jsonParams.find(name);
    if (jt != _jsonParams.end()) {
        return jt->second;
    }
    auto kt = _cookies.find(name);
    if (kt != _cookies.end()) {
        return kt->second;
    }
    return "";
}

bool HTTPRequest::hasParam(const std::string& name) const {
    return _queryParams.count(name) > 0 || _jsonParams.count(name) > 0 || _cookies.count(name) > 0;
}

std::string HTTPRequest::getHeader(std::string_view key) const {
    auto it = _headers.find(key);
    return it != _headers.end() ? it->second : "";
}

std::string_view HTTPRequest::getHeaderView(std::string_view key) const {
    auto it = _headers.find(key);
    if (it == _headers.end()) {
        return {};
    }
    return std::string_view(it->second);
}

bool HTTPRequest::hasHeader(std::string_view key) const { return _headers.find(key) != _headers.end(); }

std::shared_ptr<db::DatabaseClient> HTTPRequest::database() const { return _databaseClient; }

void HTTPRequest::parseFromWire(std::string_view raw) {
    const std::string_view requestLine = firstLine(raw);
    _rawRequestLine = std::string(requestLine);

    const std::size_t sp1 = requestLine.find(' ');
    if (sp1 == std::string_view::npos) {
        parseHeadersAndBody(raw);
        return;
    }
    const std::size_t sp2 = requestLine.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos) {
        _method = std::string(requestLine.substr(0, sp1));
        parsePathAndParams(requestLine.substr(sp1 + 1));
    } else {
        _method = std::string(requestLine.substr(0, sp1));
        parsePathAndParams(requestLine.substr(sp1 + 1, sp2 - sp1 - 1));
    }

    parseHeadersAndBody(raw);
}

void HTTPRequest::parsePathAndParams(std::string_view pathWithQuery) {
    const std::size_t qm = pathWithQuery.find('?');
    std::string_view pathOnly = pathWithQuery;
    std::string_view queryString;

    if (qm != std::string_view::npos) {
        pathOnly = pathWithQuery.substr(0, qm);
        queryString = pathWithQuery.substr(qm + 1);
    }

    _path = std::string(pathOnly);
    _pathParts = splitPathSegments(pathOnly);

    while (!queryString.empty()) {
        std::size_t amp = queryString.find('&');
        std::string_view p = queryString.substr(0, amp);
        if (amp == std::string_view::npos) {
            queryString = {};
        } else {
            queryString.remove_prefix(amp + 1);
        }
        const std::size_t eqPos = p.find('=');
        if (eqPos != std::string_view::npos) {
            const std::string key = urlDecode(p.substr(0, eqPos));
            const std::string val = urlDecode(p.substr(eqPos + 1));
            _queryParams[key] = val;
        } else if (!p.empty()) {
            _queryParams[urlDecode(p)] = "";
        }
    }
}

void HTTPRequest::parseHeadersAndBody(std::string_view rawRequest) {
    const auto split = splitHttpHeaders(rawRequest);
    if (split.has_value()) {
        _bodyView = rawRequest.substr(split->headerSectionEnd);
        parseHeaders(rawRequest.substr(0, split->headerSectionEnd - split->delimiterLength));
    } else {
        _bodyView = {};
        parseHeaders(rawRequest);
    }

    bool isJsonBody = false;
    auto it = _headers.find(std::string_view("content-type"));
    if (it != _headers.end()) {
        const std::string& ct = it->second;
        if (ct.find("application/json") != std::string::npos) {
            isJsonBody = true;
        }
    }

    if (isJsonBody && !_bodyView.empty()) {
        parseJsonBody();
    }

    auto originIt = _headers.find(std::string_view("origin"));
    if (originIt != _headers.end()) {
        origin = originIt->second;
    }

    auto cookieIt = _headers.find(std::string_view("cookie"));
    if (cookieIt != _headers.end()) {
        parseCookies(std::string_view(cookieIt->second));
    }
}

void HTTPRequest::parseHeaders(std::string_view headerSection) {
    std::string_view rest = advancePastFirstLine(headerSection);
    const size_t estimatedHeaders = 1 + std::count(rest.begin(), rest.end(), '\n');
    _headers.reserve(_headers.size() + estimatedHeaders);
    std::string keyBuf;
    std::string valueBuf;
    keyBuf.reserve(48);
    valueBuf.reserve(96);

    while (!rest.empty()) {
        std::size_t lineEnd = rest.find('\n');
        std::string_view line = rest.substr(0, lineEnd);
        if (lineEnd == std::string_view::npos) {
            rest = {};
        } else {
            rest.remove_prefix(lineEnd + 1);
        }
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const std::size_t colonPos = line.find(':');
        if (colonPos == std::string_view::npos) {
            continue;
        }
        appendLowerNoWhitespace(keyBuf, line.substr(0, colonPos));
        valueBuf.assign(stripQuotesSv(trimSv(line.substr(colonPos + 1))));
        _headers.insert_or_assign(std::move(keyBuf), std::move(valueBuf));
    }
}

void HTTPRequest::parseCookies(std::string_view cookieHeader) {
    while (!cookieHeader.empty()) {
        std::size_t semi = cookieHeader.find(';');
        std::string_view cookie = cookieHeader.substr(0, semi);
        if (semi == std::string_view::npos) {
            cookieHeader = {};
        } else {
            cookieHeader.remove_prefix(semi + 1);
        }
        cookie = trimSv(cookie);
        const std::size_t eqPos = cookie.find('=');
        if (eqPos != std::string_view::npos) {
            const std::string key = trim(cookie.substr(0, eqPos));
            const std::string val = trim(cookie.substr(eqPos + 1));
            _cookies[key] = val;
        }
    }
}

void HTTPRequest::parseJsonBody() {
    if (_bodyView.empty()) {
        return;
    }
    JSONParser parser(_bodyView, _backing);
    for (const std::string& key : parser.getKeys()) {
        _jsonParams[key] = parser.getString(key);
    }
}

std::vector<std::string> HTTPRequest::splitPathSegments(std::string_view pathOnly) {
    std::vector<std::string> result;
    result.reserve(16);
    while (!pathOnly.empty()) {
        const std::size_t slash = pathOnly.find('/');
        std::string_view part = pathOnly.substr(0, slash);
        if (slash == std::string_view::npos) {
            pathOnly = {};
        } else {
            pathOnly.remove_prefix(slash + 1);
        }
        if (!part.empty()) {
            result.emplace_back(part);
        }
    }
    return result;
}

std::string HTTPRequest::removeWhitespace(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (unsigned char c : input) {
        if (!std::isspace(c)) {
            output.push_back(static_cast<char>(c));
        }
    }
    return output;
}

std::string HTTPRequest::trim(std::string_view input) {
    input = trimSv(input);
    return std::string(input);
}

std::string HTTPRequest::stripQuotes(std::string_view input) {
    return std::string(stripQuotesSv(input));
}

std::string HTTPRequest::toLower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (unsigned char c : str) {
        result.push_back(static_cast<char>(asciiLower(c)));
    }
    return result;
}

}  // namespace geruest
