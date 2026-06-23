#include "WildcardMatch.hpp"

namespace geruest {

namespace {

bool matchWildcard(const char* pattern, const char* text) {
    if (*pattern == '\0') {
        return *text == '\0';
    }

    if (*pattern == '*') {
        while (*pattern == '*') {
            pattern++;
        }

        if (*pattern == '\0') {
            return true;
        }

        while (*text != '\0') {
            if (matchWildcard(pattern, text)) {
                return true;
            }
            text++;
        }

        return matchWildcard(pattern, text);
    }

    if (*text != '\0' && *pattern == *text) {
        return matchWildcard(pattern + 1, text + 1);
    }

    return false;
}

}  // namespace

bool matchesWildcardPattern(const std::string& pattern, const std::string& path) {
    if (pattern.find('*') == std::string::npos) {
        return pattern == path;
    }
    return matchWildcard(pattern.c_str(), path.c_str());
}

std::optional<std::string> extractWildcardCapture(const std::string& pattern, const std::string& path) {
    const size_t firstStar = pattern.find('*');
    if (firstStar == std::string::npos) {
        if (pattern == path) {
            return std::string();
        }
        return std::nullopt;
    }

    if (pattern.find('*', firstStar + 1) != std::string::npos) {
        return std::nullopt;
    }

    const std::string prefix = pattern.substr(0, firstStar);
    const std::string suffix = pattern.substr(firstStar + 1);

    if (path.size() < prefix.size() + suffix.size()) {
        return std::nullopt;
    }

    if (path.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }

    if (!suffix.empty() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return std::nullopt;
    }

    return path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
}

std::string applyWildcardCapture(const std::string& target, const std::string& capture) {
    if (target.find('*') == std::string::npos) {
        return target;
    }

    std::string resolved;
    resolved.reserve(target.size() + capture.size());
    for (char c : target) {
        if (c == '*') {
            resolved += capture;
        } else {
            resolved += c;
        }
    }
    return resolved;
}

bool isLikelyExternalTarget(const std::string& target) {
    return target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0 ||
           target.rfind("//", 0) == 0;
}

}  // namespace geruest
