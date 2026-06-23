/**
 * @file AssetMerger.hpp
 * @date 19.12.2025
 *
 * @author Urs Behrmann
 *
 * @brief This class handles automatic merging of CSS and JS assets per page.
 * 
 * When enabled, it scans HTML templates for:
 * - <link rel="stylesheet" href="..."> tags
 * - <script src="..."></script> tags
 * 
 * It then merges all referenced files into single CSS/JS files named after the page,
 * and replaces the original tags with single includes.
 */

#ifndef GERUEST_ASSETMERGER_HPP
#define GERUEST_ASSETMERGER_HPP

#include "AssetHtmlDiscovery.hpp"

#include <string>
#include <vector>

namespace geruest {

/**
 * Result of the asset merge operation
 */
struct MergeResult {
    std::string modifiedHtml;
    std::string mergedCss;
    std::string mergedJs;
    std::vector<std::string> cssFiles;
    std::vector<std::string> jsFiles;
    std::string cssSubdir;
    std::string jsSubdir;
    bool hasCss;
    bool hasJs;
};

class AssetMerger {
public:
    AssetMerger(const std::string& serverRoot, bool removeComments = true,
                const std::vector<std::string>& exclusions = {});

    MergeResult processHtml(const std::string& htmlContent, const std::string& pageName);

    JsMergeDiscovery discoverJsMergeInputs(const std::string& htmlContent);
    CssMergeDiscovery discoverCssMergeInputs(const std::string& htmlContent);

    std::vector<std::string> predictMergedAssetUrls(const std::string& htmlContent,
                                                    const std::string& pageName);

    static std::string pageNameFromHtmlPath(const std::string& htmlFilePath);

    static std::string sitePathFromHtmlFile(const std::string& htmlRootDir,
                                            const std::string& htmlAbsolutePath);

    static std::string findHtmlTemplateByPageName(const std::string& htmlRootDir,
                                                  const std::string& pageName);

    std::string mergeCssFiles(const std::vector<std::string>& cssFiles);
    std::string mergeJsFiles(const std::vector<std::string>& jsFiles);

    static std::string removeJsComments(const std::string& content);

    std::string resolveAssetPath(const std::string& href, const std::string& assetType);

private:
    std::string _serverRoot;
    bool _removeComments;
    std::vector<std::string> _exclusions;

    bool isExcluded(const std::string& filename) const;
    static std::string loadFile(const std::string& filePath);
    static std::string removeCssComments(const std::string& content);
};

}  // namespace geruest

#endif  // GERUEST_ASSETMERGER_HPP
