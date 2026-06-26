/**
 * @file CacheBustQuery.cpp
 */

#include "CacheBustQuery.hpp"

#include <cctype>
#include <string>

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

bool hasCacheBustParam(std::string_view url) {
    const size_t qm = url.find('?');
    if (qm == std::string_view::npos) {
        return false;
    }
    std::string_view query = url.substr(qm + 1);
    while (!query.empty()) {
        const size_t amp = query.find('&');
        const std::string_view pair = query.substr(0, amp);
        const size_t eq = pair.find('=');
        const std::string_view key = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
        if (iequalsAscii(key, "v")) {
            return true;
        }
        if (amp == std::string_view::npos) {
            break;
        }
        query.remove_prefix(amp + 1);
    }
    return false;
}

bool shouldSkipCacheBustUrl(std::string_view url) {
    if (url.empty() || url[0] == '#') {
        return true;
    }
    if (url.size() >= 2 && url[0] == '/' && url[1] == '/') {
        return true;
    }
    if (url.find(':') != std::string_view::npos) {
        const std::string_view lower = url.substr(0, url.find(':'));
        if (iequalsAscii(lower, "http") || iequalsAscii(lower, "https") || iequalsAscii(lower, "data") ||
            iequalsAscii(lower, "mailto") || iequalsAscii(lower, "javascript")) {
            return true;
        }
    }
    return hasCacheBustParam(url);
}

bool isStaticAssetPath(std::string_view url) {
    const size_t qm = url.find('?');
    const size_t hash = url.find('#');
    size_t end = url.size();
    if (qm != std::string_view::npos) {
        end = std::min(end, qm);
    }
    if (hash != std::string_view::npos) {
        end = std::min(end, hash);
    }

    const std::string_view path = url.substr(0, end);
    const size_t dot = path.rfind('.');
    if (dot == std::string_view::npos || dot + 1 >= path.size()) {
        return false;
    }

    const std::string_view ext = path.substr(dot + 1);
    static constexpr const char* kExts[] = {
        "css",  "js",   "mjs",  "png",  "jpg",  "jpeg", "gif",  "svg",  "webp", "ico",
        "woff", "woff2", "ttf", "eot",  "otf",  "mp3",  "mp4",  "webm", "pdf",
    };
    for (const char* candidate : kExts) {
        if (iequalsAscii(ext, candidate)) {
            return true;
        }
    }
    return false;
}

void rewriteQuotedUrls(std::string& html, std::string_view attrPrefix, char quote, std::string_view token) {
    size_t pos = 0;
    while ((pos = html.find(attrPrefix, pos)) != std::string_view::npos) {
        const size_t valueStart = pos + attrPrefix.size();
        const size_t valueEnd = html.find(quote, valueStart);
        if (valueEnd == std::string::npos) {
            break;
        }

        const std::string_view url(html.data() + valueStart, valueEnd - valueStart);
        if (!shouldSkipCacheBustUrl(url) && isStaticAssetPath(url)) {
            const std::string busted = appendCacheBustParam(url, token);
            html.replace(valueStart, valueEnd - valueStart, busted);
            pos = valueStart + busted.size();
        } else {
            pos = valueEnd + 1;
        }
    }
}

}  // namespace

std::string appendCacheBustParam(std::string_view url, std::string_view token) {
    if (token.empty()) {
        return std::string(url);
    }

    const size_t hash = url.find('#');
    const std::string_view beforeHash = (hash == std::string_view::npos) ? url : url.substr(0, hash);
    const std::string_view fragment = (hash == std::string_view::npos) ? std::string_view{} : url.substr(hash);

    std::string out;
    out.reserve(beforeHash.size() + token.size() + 8);
    out.append(beforeHash);
    out.push_back(beforeHash.find('?') == std::string_view::npos ? '?' : '&');
    out.append("v=");
    out.append(token);
    out.append(fragment);
    return out;
}

void appendCacheBustToHtml(std::string& html, std::string_view token) {
    if (token.empty()) {
        return;
    }
    rewriteQuotedUrls(html, "href=\"", '"', token);
    rewriteQuotedUrls(html, "src=\"", '"', token);
    rewriteQuotedUrls(html, "href='", '\'', token);
    rewriteQuotedUrls(html, "src='", '\'', token);
}

}  // namespace geruest
