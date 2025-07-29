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

namespace geruest {

class HtmlBuilder : public ContentBuilder {
   public:
    HtmlBuilder(const std::string &inputPath, const std::string &inputServerRoot, bool removeCommentsFlag = true);

   private:
    void buildHtml();

    /**
     * Replace keywords in the file that start with a '{' and end with a '}'
     */
    void replaceCurlyBrackets();

    /**
     * Replace keywords in the file that start with a '[' and end with a ']'
     */
    void replaceTranslations(const std::string &language);

    /**
     * Replace the references in the file, based on the language
     */
    void replaceReferences(const std::string &language);
};

}  // namespace geruest

#endif  // HTMLBUILDER_HPP