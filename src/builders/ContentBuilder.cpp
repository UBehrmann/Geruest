/**
 * @file ContentBuilder.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is the base for all content builders
 */

#include "ContentBuilder.hpp"
#include "AssetMerger.hpp"
#include "CSSBuilder.hpp"
#include "HTMLBuilder.hpp"
#include "JSBuilder.hpp"

namespace geruest {

ContentBuilder::ContentBuilder(const std::string &inputPath, const ServerData& serverData)
    : _serverData(serverData), root(serverData.getRoot()), path(inputPath) {
    builtFile = loadFile(path);
}

std::string ContentBuilder::sizeString() const { return std::to_string(builtFile.size()); }

size_t ContentBuilder::size() const { return builtFile.size(); }

std::string ContentBuilder::file() const { return builtFile; }

std::unique_ptr<ContentBuilder> ContentBuilder::create(const std::string& contentType,
                                                       const std::string& absolutePath,
                                                       const ServerData& serverData) {
    if (contentType == "text/html") {
        return std::make_unique<HtmlBuilder>(absolutePath, serverData);
    }
    if (contentType == "text/javascript") {
        return std::make_unique<JSBuilder>(absolutePath, serverData);
    }
    if (contentType == "text/css") {
        return std::make_unique<CSSBuilder>(absolutePath, serverData);
    }
    return nullptr;
}

bool ContentBuilder::tryLoadMergedAssetDevCache(const std::string& absolutePath, const ServerData& serverData,
                                                std::string& out) {
    if (!serverData.isDevMode() || !serverData.getMergeAssets()) {
        return false;
    }
    const size_t rootPos = absolutePath.find("/assets/");
    if (rootPos == std::string::npos) {
        return false;
    }
    const std::string relativePath = absolutePath.substr(rootPos);
    if (!HtmlBuilder::hasMergedAssetInCache(relativePath)) {
        return false;
    }
    out = HtmlBuilder::getMergedAssetFromCache(relativePath);
    return true;
}

std::string ContentBuilder::loadFile(const std::string& pathReceived) {
    std::ifstream fileStream(pathReceived, std::ios::binary | std::ios::ate);

    if (!fileStream) return "";

    // Get file size and pre-allocate string
    auto fileSize = fileStream.tellg();
    if (fileSize <= 0) {
        fileStream.close();
        return "";
    }
    
    std::string content;
    content.resize(static_cast<size_t>(fileSize));
    
    // Read directly into string buffer
    fileStream.seekg(0);
    fileStream.read(&content[0], fileSize);
    fileStream.close();

    return content;
}

}  // namespace geruest


// Utility to remove comments from a string for CSS, JS, or HTML
std::string geruest::ContentBuilder::removeCommentsFromString(const std::string& content, const std::string& type) {
    if (type == FILETYPE_JS) {
        return AssetMerger::removeJsComments(content);
    }

    std::string result = content;

    // Remove /* ... */ comments (CSS)
    if (type == FILETYPE_CSS) {
        size_t start = 0;
        while ((start = result.find("/*", start)) != std::string::npos) {
            size_t end = result.find("*/", start + 2);
            if (end == std::string::npos) break;
            result.erase(start, end - start + 2);
        }
    }

    // Remove <!-- ... --> comments (HTML)
    if (type == FILETYPE_HTML) {
        size_t start = 0;
        while ((start = result.find("<!--", start)) != std::string::npos) {
            size_t end = result.find("-->", start + 4);
            if (end == std::string::npos) break;
            
            size_t eraseEnd = end + 3;
            if (eraseEnd < result.size() && result[eraseEnd] == '\n') {
                eraseEnd++;
            } else if (eraseEnd < result.size() - 1 && 
                      result[eraseEnd] == '\r' && result[eraseEnd + 1] == '\n') {
                eraseEnd += 2;
            }
            
            result.erase(start, eraseEnd - start);
        }
    }

    return result;
}
