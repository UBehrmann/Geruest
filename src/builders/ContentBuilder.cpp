/**
 * @file ContentBuilder.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is the base for all content builders
 */

#include "ContentBuilder.hpp"

namespace geruest {

ContentBuilder::ContentBuilder(std::string pathReceived, std::string serverRoot, bool removeComments)
    : root(std::move(serverRoot)), path(std::move(pathReceived)), removeComments(removeComments) {
    builtFile = loadFile(path);
}

std::string ContentBuilder::sizeString() const { return std::to_string(builtFile.size()); }

size_t ContentBuilder::size() const { return builtFile.size(); }

std::string ContentBuilder::file() const { return builtFile; }

std::string ContentBuilder::loadFile(const std::string& pathReceived) {
    std::ifstream fileStream(pathReceived);

    if (!fileStream) return "";

    std::stringstream buffer;
    buffer << fileStream.rdbuf();

    fileStream.close();

    return buffer.str();
}

}  // namespace geruest


// Utility to remove comments from a string for CSS, JS, or HTML
std::string geruest::ContentBuilder::removeCommentsFromString(const std::string& content, const std::string& type) {
    std::string result = content;

    // Remove /* ... */ comments
    if (type == FILETYPE_CSS || type == FILETYPE_JS) {
        size_t start = 0;
        while ((start = result.find("/*", start)) != std::string::npos) {
            size_t end = result.find("*/", start + 2);
            if (end == std::string::npos) break;
            result.erase(start, end - start + 2);
        }
    }

    // Remove // ... comments (single line)
    if (type == FILETYPE_JS) {
        size_t pos = 0;
        while ((pos = result.find("//", pos)) != std::string::npos) {
            size_t lineEnd = result.find('\n', pos);
            if (lineEnd == std::string::npos) {
                result.erase(pos);
                break;
            } else {
                result.erase(pos, lineEnd - pos);
            }
        }
    }

    // Remove <!-- ... --> comments
    if (type == FILETYPE_HTML) {
        size_t start = 0;
        while ((start = result.find("<!--", start)) != std::string::npos) {
            size_t end = result.find("-->", start + 4);
            if (end == std::string::npos) break;
            result.erase(start, end - start + 3);
        }
    }

    return result;
}
