/**
 * @file CSSBuilder.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the CSS files.
 */

#include "CSSBuilder.hpp"

namespace geruest {

CSSBuilder::CSSBuilder(const std::string &inputPath, const std::string &inputServerRoot, bool removeCommentsFlag) 
    : ContentBuilder(inputPath, inputServerRoot, removeCommentsFlag) {
    json = getJSONFromFile(inputServerRoot + "/files_maps/css_file_map.json");
    pageName = getFileNameWithoutExtension(path);
    builCSS();
}

std::string CSSBuilder::getFileNameWithoutExtension(const std::string& path) {
    // Find the last position of the slash or backslash
    size_t lastSlashPos = path.find_last_of('/');

    // Get the file name from the path
    std::string fileName = (lastSlashPos == std::string::npos) ? path : path.substr(lastSlashPos + 1);

    // Find the last position of the dot
    size_t lastDotPos = fileName.find_last_of('.');
    // Remove the extension from the file name
    if (lastDotPos != std::string::npos) {
        fileName = fileName.substr(0, lastDotPos);
    }

    return fileName;
}

void CSSBuilder::builCSS() {
    JSONParser jsonForFile = json->getObject(pageName);

    // If there are no files to include, return
    if(jsonForFile.getKeys().empty()) return;

    builtFile = "";

    // Build the file by including all the files
    for (const auto& key : jsonForFile.getKeys()) {
        builtFile += loadFile(root + "/assets/css" + jsonForFile.getString(key)) + "\n\n";
    }

    // Remove comments if enabled
    if (removeComments) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_CSS);
    }
}

}  // namespace geruest
