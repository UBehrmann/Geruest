/**
 * @file LanguageConfig.hpp
 * @brief i18n language list and path prefix helpers for redirects and gates.
 */

#ifndef GERUEST_LANGUAGECONFIG_HPP
#define GERUEST_LANGUAGECONFIG_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace geruest {

class LanguageConfig {
   public:
    LanguageConfig() = default;
    LanguageConfig(const LanguageConfig&) = default;
    LanguageConfig& operator=(const LanguageConfig&) = default;

    void setAvailableLanguages(const std::vector<std::string>& languages);
    const std::vector<std::string>& getAvailableLanguages() const { return _availableLanguages; }
    const std::string& getDefaultLanguage() const { return _defaultLanguage; }

    bool isLanguageAvailable(const std::string& lang) const;
    bool hasLanguages() const { return !_availableLanguages.empty(); }

    std::optional<std::string> languagePrefixFromPath(const std::string& path) const;
    std::string resolvePreferredLanguage(std::string_view acceptLanguage) const;
    std::string localizePathWithRequestLanguage(const std::string& path, const std::string& requestPath) const;

    std::optional<std::string> stripSupportedLanguagePrefix(const std::string& path) const;
    std::string normalizeRedirectTargetLanguage(const std::string& target, const std::string& requestPath) const;

   private:
    std::optional<std::string> extractSupportedLanguagePrefix(const std::string& path) const;
    bool hasSupportedLanguagePrefixInTarget(const std::string& target) const;

    std::vector<std::string> _availableLanguages;
    std::string _defaultLanguage;
};

}  // namespace geruest

#endif  // GERUEST_LANGUAGECONFIG_HPP
