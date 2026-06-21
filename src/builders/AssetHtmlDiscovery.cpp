/**
 * @file AssetHtmlDiscovery.cpp
 *
 * @brief HTML asset reference discovery (regex scan + local-path filtering).
 */

#include "AssetHtmlDiscovery.hpp"

#include <regex>

namespace geruest {

bool AssetHtmlDiscovery::isExternalUrl(const std::string& url) {
    return url.find("http://") == 0 || url.find("https://") == 0 || url.find("//") == 0;
}

std::vector<AssetReference> AssetHtmlDiscovery::extractCssReferences(const std::string& htmlContent) {
    std::vector<AssetReference> references;

    std::regex linkPattern(
        R"(<link\s+[^>]*rel\s*=\s*["']stylesheet["'][^>]*href\s*=\s*["']([^"']+)["'][^>]*>|)"
        R"(<link\s+[^>]*href\s*=\s*["']([^"']+)["'][^>]*rel\s*=\s*["']stylesheet["'][^>]*>)",
        std::regex::icase);

    std::string::const_iterator searchStart(htmlContent.cbegin());
    std::smatch match;

    while (std::regex_search(searchStart, htmlContent.cend(), match, linkPattern)) {
        AssetReference ref;
        ref.href = match[1].matched ? match[1].str() : match[2].str();
        ref.startPos = static_cast<size_t>(match.position()) +
                       static_cast<size_t>(std::distance(htmlContent.cbegin(), searchStart));
        ref.endPos = ref.startPos + match.length();
        ref.isExternal = isExternalUrl(ref.href);
        references.push_back(ref);
        searchStart = match.suffix().first;
    }

    return references;
}

std::vector<AssetReference> AssetHtmlDiscovery::extractJsReferences(const std::string& htmlContent) {
    std::vector<AssetReference> references;

    std::regex scriptPattern(
        R"(<script\s+[^>]*src\s*=\s*["']([^"']+)["'][^>]*>\s*</script>|)"
        R"(<script\s+[^>]*src\s*=\s*["']([^"']+)["'][^>]*/>)",
        std::regex::icase);

    std::string::const_iterator searchStart(htmlContent.cbegin());
    std::smatch match;

    while (std::regex_search(searchStart, htmlContent.cend(), match, scriptPattern)) {
        AssetReference ref;
        ref.href = match[1].matched ? match[1].str() : match[2].str();
        ref.startPos = static_cast<size_t>(match.position()) +
                       static_cast<size_t>(std::distance(htmlContent.cbegin(), searchStart));
        ref.endPos = ref.startPos + match.length();
        ref.isExternal = isExternalUrl(ref.href);
        references.push_back(ref);
        searchStart = match.suffix().first;
    }

    return references;
}

std::string AssetHtmlDiscovery::mergedAssetBundleRelPath(const std::string& pageName,
                                                         const std::string& subdir,
                                                         const char* ext) {
    if (subdir.empty()) {
        return pageName + ext;
    }
    return subdir + "/" + pageName + ext;
}

std::string AssetHtmlDiscovery::mergedAssetSitePath(const std::string& pageName, const std::string& subdir,
                                                    const char* ext) {
    return "/" + mergedAssetBundleRelPath(pageName, subdir, ext);
}

JsMergeDiscovery AssetHtmlDiscovery::filterLocalJsRefs(
    const std::vector<AssetReference>& refs,
    const std::function<std::string(const std::string& href)>& resolvePath,
    const std::function<bool(const std::string& absPath)>& pathExists,
    const std::function<bool(const std::string& href)>& isExcluded) {
    JsMergeDiscovery d;
    d.allJsRefs = refs;
    for (const auto& ref : refs) {
        if (ref.isExternal || isExcluded(ref.href)) {
            continue;
        }
        std::string filePath = resolvePath(ref.href);
        if (pathExists(filePath)) {
            d.localJsAbsolutePaths.push_back(std::move(filePath));
            d.jsHrefs.push_back(ref.href);
            if (d.jsSubdir.empty()) {
                const size_t lastSlash = ref.href.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    d.jsSubdir = ref.href.substr(0, lastSlash);
                }
            }
        }
    }
    d.hasJs = !d.localJsAbsolutePaths.empty();
    return d;
}

CssMergeDiscovery AssetHtmlDiscovery::filterLocalCssRefs(
    const std::vector<AssetReference>& refs,
    const std::function<std::string(const std::string& href)>& resolvePath,
    const std::function<bool(const std::string& absPath)>& pathExists,
    const std::function<bool(const std::string& href)>& isExcluded) {
    CssMergeDiscovery d;
    d.allCssRefs = refs;
    for (const auto& ref : refs) {
        if (ref.isExternal || isExcluded(ref.href)) {
            continue;
        }
        std::string filePath = resolvePath(ref.href);
        if (pathExists(filePath)) {
            d.localCssAbsolutePaths.push_back(std::move(filePath));
            d.cssHrefs.push_back(ref.href);
            if (d.cssSubdir.empty()) {
                const size_t lastSlash = ref.href.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    d.cssSubdir = ref.href.substr(0, lastSlash);
                }
            }
        }
    }
    d.hasCss = !d.localCssAbsolutePaths.empty();
    return d;
}

}  // namespace geruest
