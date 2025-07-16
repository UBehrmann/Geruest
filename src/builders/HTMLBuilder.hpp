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
#include <string>

#include "ContentBuilder.hpp"
#include "FileManagement/FileManagement.hpp"
#include "parser/JSONParser.hpp"

namespace fs = std::filesystem;

class HtmlBuilder : public ContentBuilder {
   public:
    HtmlBuilder(const std::string &path, const std::string &serverRoot) : ContentBuilder(path, serverRoot) {
        buildHtml();
    }

   private:
    void buildHtml() {
        // TODO : Commented that the webpages are always rebuilt till this part is tested
        //        if (!Config::isDev() && !builtFile.empty()) {
        //            // test if the file is older than X hours
        //            if (!FileManagement::isOlderThan(path, 24)){
        //                // File is not older than 24 hours, no need to rebuild
        //                return;
        //            }
        //        }

        // Build the HTML file

        auto rootSize = (unsigned)root.size();
        unsigned langStart = rootSize + 6;
        unsigned langSize = 2;

        std::cout << "Building HTML file: " << path << std::endl;

        // Get language from path
        std::string language = path.substr(langStart, langSize);

        // Get the template path
        std::string templatePath = path;

        // Load template file, remove language redirection
        templatePath.erase(langStart, langSize + 1);  // remove the language part

		std::cout << "Template path: " << templatePath << std::endl;

        builtFile = loadFile(templatePath);

        // Check if template file was loaded
        if (builtFile.empty()) return;

        std::cout << "Building HTML file 2." << std::endl;

        // Remplace keywords with content
        // TODO : find a better names for this function!
        replaceCurlyBrackets();

        // Replace text with correct language
        replaceTranslations(language);

        // Change references to the correct path
        replaceReferences(language);

        std::cout << "Building HTML file end." << std::endl;

        // Save the file
        if (!FileManagement::saveFile(path, builtFile)) {
            // Error saving file
            builtFile = "";
        }
    }

    /**
     * Replace keywords in the file that start with a '{' and end with a '}'
     */
    void replaceCurlyBrackets() {
        size_t startPos;
        size_t endPos = 0;

        while ((startPos = builtFile.find('{', endPos + 1)) != std::string::npos) {
            endPos = builtFile.find('}', startPos + 1);

            if (endPos == std::string::npos) {
                endPos = builtFile.size();
            }

            std::string keyword = builtFile.substr(startPos, endPos - startPos);
            std::string pathToInsert = root + keyword.substr(1);  // remove the '}' character

            std::string toInsert;

            if (fs::exists(pathToInsert)) {
                toInsert = loadFile(pathToInsert);
            }

            builtFile.replace(startPos, keyword.size() + 1, toInsert);
        }
    }

    /**
     * Replace keywords in the file that start with a '[' and end with a ']'
     */
    void replaceTranslations(const std::string &language) {
        size_t startPos = 0;

        while ((startPos = builtFile.find('[', startPos)) != std::string::npos) {
            size_t endPos = builtFile.find(']', startPos + 1);

            if (endPos == std::string::npos) break;

            std::string keyword = builtFile.substr(startPos, endPos - startPos);
            std::string pathToInsert = root + keyword.substr(1);  // remove the '[' character

            // path_to_json:element
            std::string pathToJSON = pathToInsert.substr(0, pathToInsert.find(':'));
            std::string element = pathToInsert.substr(pathToInsert.find(':') + 1);

            std::string toInsert;

            if (fs::exists(pathToJSON)) {
                JSONParser *jsonParser = getJSONFromFile(pathToJSON);

                // Get the right language
                JSONParser languageArray = jsonParser->getObject(language);

                // If language doesn't exist, use English
                if (languageArray.getKeys().empty()) {
                    languageArray = jsonParser->getObject("en");
                }

                delete jsonParser;

                // Get the element
                toInsert = languageArray.getString(element);
            }

            builtFile.replace(startPos, keyword.size() + 1, toInsert);
            startPos += toInsert.size();
        }
    }

    /**
     * Replace the references in the file, based on the language
     */
    void replaceReferences(const std::string &language) {
        // Find if there are references in the file
        // href="/about_me"
        // change to href="/en/about_me"

        size_t pos = 0;
        const std::string prefix = "href=\"/";

        while ((pos = builtFile.find(prefix, pos)) != std::string::npos) {
            size_t start = pos + prefix.length();  // position after href="/

            // Skip if it already starts with the language
            if (builtFile.compare(start, language.length() + 1, language + "/") != 0) {
                builtFile.insert(start, language + "/");
                pos = start + language.length() + 1;  // Move past inserted /lang/
            } else {
                pos = start + language.length() + 1;  // Already has /lang/, skip
            }
        }
    }
};

#endif  // HTMLBUILDER_HPP