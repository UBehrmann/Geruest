/**
 * @file AssetHtmlDiscovery.hpp
 *
 * @brief Pure HTML scanning for CSS/JS asset references (no merge file-body I/O).
 */

#ifndef GERUEST_ASSETHTMLDISCOVERY_HPP
#define GERUEST_ASSETHTMLDISCOVERY_HPP

#include <functional>
#include <string>
#include <vector>

namespace geruest {

struct AssetReference {
    std::string href;
    size_t startPos = 0;
    size_t endPos = 0;
    bool isExternal = false;
};

/** Local JS merge inputs from HTML; no merge I/O. */
struct JsMergeDiscovery {
    bool hasJs = false;
    std::vector<std::string> jsHrefs;
    std::vector<std::string> localJsAbsolutePaths;
    std::string jsSubdir;
    std::vector<AssetReference> allJsRefs;
};

/** Local CSS merge inputs from HTML; no merge I/O. */
struct CssMergeDiscovery {
    bool hasCss = false;
    std::vector<std::string> cssHrefs;
    std::vector<std::string> localCssAbsolutePaths;
    std::string cssSubdir;
    std::vector<AssetReference> allCssRefs;
};

struct AssetHtmlDiscovery {
    AssetHtmlDiscovery() = delete;

    static bool isExternalUrl(const std::string& url);
    static std::vector<AssetReference> extractCssReferences(const std::string& htmlContent);
    static std::vector<AssetReference> extractJsReferences(const std::string& htmlContent);

    /** Site path, e.g. "/page.css" or "/subdir/page.css". */
    static std::string mergedAssetSitePath(const std::string& pageName, const std::string& subdir,
                                           const char* ext);

    /** Bundle relative path without leading slash, e.g. "page.css" or "subdir/page.css". */
    static std::string mergedAssetBundleRelPath(const std::string& pageName, const std::string& subdir,
                                                const char* ext);

    static JsMergeDiscovery filterLocalJsRefs(
        const std::vector<AssetReference>& refs,
        const std::function<std::string(const std::string& href)>& resolvePath,
        const std::function<bool(const std::string& absPath)>& pathExists,
        const std::function<bool(const std::string& href)>& isExcluded);

    static CssMergeDiscovery filterLocalCssRefs(
        const std::vector<AssetReference>& refs,
        const std::function<std::string(const std::string& href)>& resolvePath,
        const std::function<bool(const std::string& absPath)>& pathExists,
        const std::function<bool(const std::string& href)>& isExcluded);
};

}  // namespace geruest

#endif  // GERUEST_ASSETHTMLDISCOVERY_HPP
