#include "LanguageConfig.hpp"

#include "WildcardMatch.hpp"

#include <algorithm>
#include <cctype>

namespace geruest {

void LanguageConfig::setAvailableLanguages(const std::vector<std::string>& languages) {
    _availableLanguages = languages;
    if (!languages.empty()) {
        _defaultLanguage = languages[0];
    }
}

bool LanguageConfig::isLanguageAvailable(const std::string& lang) const {
    return std::find(_availableLanguages.begin(), _availableLanguages.end(), lang) != _availableLanguages.end();
}

std::optional<std::string> LanguageConfig::extractSupportedLanguagePrefix(const std::string& path) const {
    if (path.size() < 3 || path[0] != '/') {
        return std::nullopt;
    }

    const auto slashPos = path.find('/', 1);
    const size_t langLen = (slashPos == std::string::npos) ? (path.size() - 1) : (slashPos - 1);
    if (langLen != 2) {
        return std::nullopt;
    }

    const char c1 = path[1];
    const char c2 = path[2];
    if (!std::isalpha(static_cast<unsigned char>(c1)) || !std::isalpha(static_cast<unsigned char>(c2))) {
        return std::nullopt;
    }

    const std::string lang = path.substr(1, 2);
    if (!isLanguageAvailable(lang)) {
        return std::nullopt;
    }

    return lang;
}

bool LanguageConfig::hasSupportedLanguagePrefixInTarget(const std::string& target) const {
    return extractSupportedLanguagePrefix(target).has_value();
}

std::optional<std::string> LanguageConfig::languagePrefixFromPath(const std::string& path) const {
    return extractSupportedLanguagePrefix(path);
}

std::string LanguageConfig::resolvePreferredLanguage(std::string_view acceptLanguage) const {
    std::string preferredLang = _defaultLanguage;
    if (!hasLanguages() || acceptLanguage.empty()) {
        return preferredLang;
    }
    for (const auto& lang : _availableLanguages) {
        if (acceptLanguage.find(lang) != std::string_view::npos) {
            preferredLang = lang;
            break;
        }
    }
    return preferredLang;
}

std::string LanguageConfig::localizePathWithRequestLanguage(const std::string& path,
                                                            const std::string& requestPath) const {
    if (!hasLanguages()) {
        return path;
    }
    const auto requestLang = extractSupportedLanguagePrefix(requestPath);
    if (!requestLang.has_value()) {
        return path;
    }
    if (hasSupportedLanguagePrefixInTarget(path)) {
        return path;
    }
    std::string localized = path;
    if (!localized.empty() && localized[0] != '/') {
        localized = "/" + localized;
    }
    return "/" + *requestLang + localized;
}

std::optional<std::string> LanguageConfig::stripSupportedLanguagePrefix(const std::string& path) const {
    const auto lang = extractSupportedLanguagePrefix(path);
    if (!lang.has_value()) {
        return std::nullopt;
    }

    if (path.size() == 3) {
        return std::string("/");
    }

    return path.substr(3);
}

std::string LanguageConfig::normalizeRedirectTargetLanguage(const std::string& target,
                                                            const std::string& requestPath) const {
    if (!hasLanguages() || target.empty()) {
        return target;
    }

    if (isLikelyExternalTarget(target)) {
        return target;
    }

    if (target[0] != '/') {
        return target;
    }

    if (hasSupportedLanguagePrefixInTarget(target)) {
        return target;
    }

    const auto requestLang = extractSupportedLanguagePrefix(requestPath);
    if (!requestLang.has_value()) {
        return target;
    }

    if (target == "/") {
        return "/" + *requestLang + "/";
    }

    return "/" + *requestLang + target;
}

}  // namespace geruest
