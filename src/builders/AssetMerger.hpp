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

#include <string>
#include <vector>
#include <regex>

namespace geruest {

/**
 * Structure to hold information about extracted asset references
 */
struct AssetReference {
    std::string href;           // The href/src value from the tag
    size_t startPos;           // Start position of the tag in HTML
    size_t endPos;             // End position of the tag in HTML
    bool isExternal;           // True if href starts with http:// or https://
};

/**
 * Result of the asset merge operation
 */
struct MergeResult {
    std::string modifiedHtml;       // HTML with asset tags replaced
    std::string mergedCss;          // Combined CSS content
    std::string mergedJs;           // Combined JS content
    std::vector<std::string> cssFiles;  // List of CSS files that were merged
    std::vector<std::string> jsFiles;   // List of JS files that were merged
    std::string cssSubdir;          // Subdirectory for CSS files (e.g., "subfoldertest" from "/assets/css/subfoldertest/file.css")
    std::string jsSubdir;           // Subdirectory for JS files
    bool hasCss;                    // True if any CSS files were found
    bool hasJs;                     // True if any JS files were found
};

class AssetMerger {
public:
    /**
     * Constructor
     * @param serverRoot The root directory of the website
     * @param removeComments Whether to remove comments from merged assets
     * @param exclusions List of filenames to exclude from merging
     */
    AssetMerger(const std::string& serverRoot, bool removeComments = true, 
                const std::vector<std::string>& exclusions = {});

    /**
     * Process HTML content to extract and merge CSS/JS assets
     * @param htmlContent The original HTML content
     * @param pageName The name of the page (used for merged file names)
     * @return MergeResult containing modified HTML and merged assets
     */
    MergeResult processHtml(const std::string& htmlContent, const std::string& pageName);

    /**
     * Extract all CSS <link> references from HTML
     * @param htmlContent The HTML to scan
     * @return Vector of AssetReference for each CSS link found
     */
    std::vector<AssetReference> extractCssReferences(const std::string& htmlContent);

    /**
     * Extract all JS <script> references from HTML
     * @param htmlContent The HTML to scan
     * @return Vector of AssetReference for each script found
     */
    std::vector<AssetReference> extractJsReferences(const std::string& htmlContent);

    /**
     * Merge multiple CSS files into one
     * @param cssFiles Vector of file paths to merge
     * @return Combined CSS content
     */
    std::string mergeCssFiles(const std::vector<std::string>& cssFiles);

    /**
     * Merge multiple JS files into one
     * @param jsFiles Vector of file paths to merge
     * @return Combined JS content
     */
    std::string mergeJsFiles(const std::vector<std::string>& jsFiles);

private:
    std::string _serverRoot;
    bool _removeComments;
    std::vector<std::string> _exclusions;

    /**
     * Check if a file should be excluded from merging
     * @param filename The filename to check
     * @return true if file is excluded
     */
    bool isExcluded(const std::string& filename) const;

    /**
     * Load a file's content
     * @param filePath Path to the file
     * @return File content as string
     */
    static std::string loadFile(const std::string& filePath);

    /**
     * Check if a URL is external (starts with http:// or https://)
     * @param url The URL to check
     * @return true if external
     */
    static bool isExternalUrl(const std::string& url);

    /**
     * Remove comments from CSS content
     * @param content CSS content
     * @return Content with comments removed
     */
    static std::string removeCssComments(const std::string& content);

    /**
     * Remove comments from JS content
     * @param content JS content
     * @return Content with comments removed
     */
    static std::string removeJsComments(const std::string& content);

    /**
     * Resolve a relative path to an absolute path based on server root
     * @param href The href value from the tag
     * @param assetType "css" or "js" to determine subdirectory
     * @return Absolute file path
     */
    std::string resolveAssetPath(const std::string& href, const std::string& assetType);
};

}  // namespace geruest

#endif  // GERUEST_ASSETMERGER_HPP
