/**
 * @file ContentBuilder.hpp
 * @date 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is the base for all content builders
 */

#ifndef CONTENTBUILDER_HPP
#define CONTENTBUILDER_HPP

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "data/ServerData.hpp"

namespace geruest {

class ContentBuilder {
public:

    ContentBuilder(const std::string &inputPath, const ServerData& serverData);

    [[nodiscard]] std::string sizeString() const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] std::string file() const;

    static std::unique_ptr<ContentBuilder> create(const std::string& contentType, const std::string& absolutePath,
                                                  const ServerData& serverData);

    /** Dev-mode merged asset from HtmlBuilder cache; returns true when out is set. */
    static bool tryLoadMergedAssetDevCache(const std::string& absolutePath, const ServerData& serverData,
                                           std::string& out);

protected:

    // File type constants
    static constexpr const char* FILETYPE_JS = "js";
    static constexpr const char* FILETYPE_HTML = "html";
    static constexpr const char* FILETYPE_CSS = "css";

    const ServerData& _serverData;
    const std::string root;
    std::string builtFile;
    std::string path;

    /**
     * Remove comments from a string (CSS, JS, or HTML style)
     * @param content The input string
     * @param type The file type: "css", "js", or "html"
     * @return The string with comments removed
     */
    static std::string removeCommentsFromString(const std::string& content, const std::string& type);

    /**
     * Load a file
     *
     * @param pathReceived
     * @return The content of the file
     */
    [[nodiscard]] static std::string loadFile(const std::string& pathReceived);
};

}  // namespace geruest

#endif //CONTENTBUILDER_HPP