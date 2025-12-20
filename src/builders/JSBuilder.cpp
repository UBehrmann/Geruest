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
#include <filesystem>

namespace geruest {

JSBuilder::JSBuilder(const std::string &inputPath, const std::string &inputServerRoot, 
                     bool removeCommentsFlag, bool mergeAssets) 
    : ContentBuilder(inputPath, inputServerRoot, removeCommentsFlag), 
      _mergeAssets(mergeAssets) {
    builJS();
}

void JSBuilder::builJS() {
    // File is already loaded by ContentBuilder base class via loadFile(path)
    // Just handle comment removal if enabled
    if (removeComments && !builtFile.empty()) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_JS);
    }
}

}  // namespace geruest
