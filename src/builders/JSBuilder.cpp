/**
 * @file JSBuilder.cpp
 * @date on: 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * 
 * When mergeAssets=false: Serves individual JS files as-is.
 * When mergeAssets=true: Serves pre-generated merged JS files created by HTMLBuilder.
 */

#include "JSBuilder.hpp"
#include "HTMLBuilder.hpp"
#include <filesystem>

namespace geruest {

JSBuilder::JSBuilder(const std::string &inputPath, const std::string &inputServerRoot, 
                     bool removeCommentsFlag, bool mergeAssets, bool devModeFlag) 
    : ContentBuilder(inputPath, inputServerRoot, removeCommentsFlag, {}, devModeFlag), 
      _mergeAssets(mergeAssets) {
    builJS();
}

void JSBuilder::builJS() {
    // In dev mode with merging, check if content is in cache first
    if (devMode && _mergeAssets) {
        // Extract relative path from full path (remove root)
        std::string relativePath = path;
        size_t rootPos = relativePath.find("/assets/");
        if (rootPos != std::string::npos) {
            relativePath = relativePath.substr(rootPos);
            
            if (HtmlBuilder::hasMergedAssetInCache(relativePath)) {
                builtFile = HtmlBuilder::getMergedAssetFromCache(relativePath);
                return;
            }
        }
    }
    
    // File is already loaded by ContentBuilder base class via loadFile(path)
    // Just handle comment removal if enabled
    if (removeComments && !builtFile.empty()) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_JS);
    }
}

}  // namespace geruest
