/**
 * @file HTMLBuilder.hpp
 * @date 11.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build HTML files.
 * It looks for keywords in the file and replaces them with the content of the file they point to.
 */

#ifndef HTMLBUILDER_HPP
#define HTMLBUILDER_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

#include "ContentBuilder.hpp"
#include "FileManagement/FileManagement.hpp"
#include "parser/JSONParser.hpp"
#include "WebPConverter.hpp"

namespace fs = std::filesystem;

namespace geruest {

class HtmlBuilder : public ContentBuilder {
   public:
    HtmlBuilder(const std::string& inputPath, const ServerData& serverData);

    // Static cache for merged assets in dev mode
    static std::string getMergedAssetFromCache(const std::string& path);
    static bool hasMergedAssetInCache(const std::string& path);

    // Static accessor for WebP cache (used by Handler)
    static std::shared_ptr<const std::vector<uint8_t>> getWebPFromCache(const std::string& path);
    static bool hasWebPInCache(const std::string& path);

   private:
    // In-memory cache for merged assets (dev mode only)
    static std::unordered_map<std::string, std::string> _mergedAssetsCache;
    static std::mutex _cacheMutex;

    void buildHtml();

    /**
     * Replace keywords in the file that start with a '{' and end with a '}'
     */
    void replaceCurlyBrackets();

    /**
     * Replace keywords in the file that start with a '[' and end with a ']'
     */
    void replaceTranslations(const std::string& language);

    /**
     * Replace the references in the file, based on the language
     */
    void replaceReferences(const std::string& language);

    /**
     * Ensure all CSS and JS asset paths have leading slashes
     * This makes paths absolute so they work from any page depth
     */
    void ensureAbsoluteAssetPaths();

    /**
     * Process CSS and JS asset merging using AssetMerger
     * @param pageName The name of the page (for merged file naming)
     */
    void processAssetMerging(const std::string& pageName);

    /**
     * Extract the page name from the file path
     * @param filePath The full file path
     * @return The page name without extension
     */
    static std::string getPageNameFromPath(const std::string& filePath);

    /**
     * Process PNG/JPG to WebP conversion for images referenced in HTML
     * This method:
     * 1. Extracts all image paths (.png, .jpg, .jpeg) from the HTML
     * 2. Converts each image to WebP format
     * 3. Replaces image references in HTML with .webp extensions
     * 4. In devMode: caches converted images in memory
     * 5. In production: saves converted images to disk
     */
    void processWebPConversion();
};

}  // namespace geruest

#endif  // HTMLBUILDER_HPP