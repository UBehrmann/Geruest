/**
 * @file CSSBuilder.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the CSS files.
 * 
 * When mergeAssets=false: Serves individual CSS files as-is.
 * When mergeAssets=true: Serves pre-generated merged CSS files created by HTMLBuilder.
 */

#include "CSSBuilder.hpp"
#include "HTMLBuilder.hpp"
#include <filesystem>

namespace geruest {

CSSBuilder::CSSBuilder(const std::string &inputPath, const ServerData& serverData) 
    : ContentBuilder(inputPath, serverData) {
    builCSS();
}

void CSSBuilder::builCSS() {
    if (tryLoadMergedAssetDevCache(path, _serverData, builtFile)) {
        return;
    }
    
    // File is already loaded by ContentBuilder base class via loadFile(path)
    // Just handle comment removal if enabled
    if (_serverData.getRemoveComments() && !builtFile.empty()) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_CSS);
    }
}

}  // namespace geruest
