/**
 * @file JSBuilder.hpp
 * @date on: 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * It looks for the page name in the map and includes the files that are associated with it.
 * If the page name is not found, it returns the JS files associated with the path.
 */

#ifndef JSBUILDER_HPP
#define JSBUILDER_HPP

#include "ContentBuilder.hpp"
#include <string>
#include <vector>
#include "parser/JSONParser.hpp"

class JSBuilder : public ContentBuilder {
public:

    JSBuilder(const std::string& path, const std::string& serverRoot) : ContentBuilder(path, serverRoot) {

        json = getJSONFromFile(serverRoot + "/files_maps/js_file_map.json");

        pageName = getFileNameWithoutExtension(path);

        builJS();
    }

private:

    JSONParser* json;

    std::string pageName;

    static std::string getFileNameWithoutExtension(const std::string& path) {
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

    void builJS() {

        JSONParser jsonForFile = json->getObject(pageName);

        // If there are no files to include, return
        if(jsonForFile.getKeys().empty()) return;

        builtFile = "";

        // Build the file by including all the files
        for (const auto& key : jsonForFile.getKeys()) {
            builtFile += loadFile(root + "/assets/js" + jsonForFile.getString(key)) + "\n\n";
        }
    }
};

#endif //JSBUILDER_HPP