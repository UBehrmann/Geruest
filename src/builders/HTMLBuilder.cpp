/**
 * @file HTMLBuilder.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build HTML files.
 * It looks for keywords in the file and replaces them with the content of the file they point to.
 */

#include "HTMLBuilder.hpp"
#include "AssetMerger.hpp"

#include <algorithm>
#include <filesystem>

namespace geruest {

HtmlBuilder::HtmlBuilder(const std::string& inputPath, const std::string& inputServerRoot, bool removeCommentsFlag,
                         const std::vector<std::string>& languages, bool mergeAssets)
    : ContentBuilder(inputPath, inputServerRoot, removeCommentsFlag, languages), _mergeAssets(mergeAssets) {
    buildHtml();
}

void HtmlBuilder::buildHtml() {
    // TODO : Commented that the webpages are always rebuilt till this part is tested
    //        if (!Config::isDev() && !builtFile.empty()) {
    //            // test if the file is older than X hours
    //            if (!FileManagement::isOlderThan(path, 24)){
    //                // File is not older than 24 hours, no need to rebuild
    //                return;
    //            }
    //        }

    // Build the HTML file

    // Extract language from path more robustly
    // Path format: /root/html/LANG/file.html or /root/html/file.html
    std::string language;
    std::string templatePath = path;
    
    // Find /html/ in the path
    size_t htmlPos = path.find("/html/");
    if (htmlPos != std::string::npos) {
        size_t afterHtml = htmlPos + 6; // Position after "/html/"
        size_t nextSlash = path.find('/', afterHtml);
        
        if (nextSlash != std::string::npos) {
            // Extract potential language code
            std::string potentialLang = path.substr(afterHtml, nextSlash - afterHtml);
            
            // Check if it's a 2-character language code
            if (potentialLang.length() == 2 && std::isalpha(potentialLang[0]) && std::isalpha(potentialLang[1])) {
                language = potentialLang;
            }
        }
    }
    
    // If no language detected, use default language
    if (language.empty() && !availableLanguages.empty()) {
        language = availableLanguages[0];
    }

    // Check if the file exists with the language directory
    bool useLanguageSpecificFile = std::filesystem::exists(path);

    if (useLanguageSpecificFile) {
        // File exists with language directory, use it directly
        builtFile = loadFile(path);
    } else {
        // Load template file - remove language part from path
        if (!language.empty() && htmlPos != std::string::npos) {
            // Remove /LANG/ from the path to get template path
            size_t langStart = htmlPos + 6; // After "/html/"
            size_t langEnd = path.find('/', langStart);
            if (langEnd != std::string::npos) {
                templatePath = path.substr(0, langStart) + path.substr(langEnd + 1);
            }
        }
        builtFile = loadFile(templatePath);
    }

    // Remove comments if enabled
    if (removeComments) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_HTML);
    }

    // Check if template file was loaded
    if (builtFile.empty()) return;

    // Only process templates and save if using template system (not language-specific files)
    if (!useLanguageSpecificFile) {
        // Check if the target language is supported before processing and saving
        // Extract language from path: /root/html/XX/file.html
        bool languageSupported =
            availableLanguages.empty() ||
            std::find(availableLanguages.begin(), availableLanguages.end(), language) != availableLanguages.end();

        if (!languageSupported) {
            // Language not supported, don't save the file
            // Just serve the base template without language-specific processing
            return;
        }

        // Remplace keywords with content
        // TODO : find a better names for this function!
        replaceCurlyBrackets();

        // Replace text with correct language
        replaceTranslations(language);

        // Process CSS/JS asset merging if enabled
        if (_mergeAssets) {
            std::string pageName = getPageNameFromPath(path);
            processAssetMerging(pageName);
        } else {
            // When merging is disabled, ensure all asset paths have leading slashes
            ensureAbsoluteAssetPaths();
        }

        // Change references to the correct path
        replaceReferences(language);

        // Save the file
        if (!FileManagement::saveFile(path, builtFile)) {
            // Error saving file
            builtFile = "";
        }
    }
}

void HtmlBuilder::replaceCurlyBrackets() {
    size_t startPos;
    size_t endPos = 0;
    size_t scriptStart = 0;
    size_t scriptEnd = 0;

    while ((startPos = builtFile.find('{', endPos + 1)) != std::string::npos) {
        // Check if startPos is inside a <script>...</script> or <style>...</style> block
        bool insideScript = false;
        bool insideStyle = false;

        scriptStart = builtFile.rfind("<script", startPos);
        if (scriptStart != std::string::npos) {
            scriptEnd = builtFile.find("</script>", scriptStart);
            if (scriptEnd != std::string::npos && startPos > scriptStart && startPos < scriptEnd) {
                insideScript = true;
            }
        }

        size_t styleStart = builtFile.rfind("<style", startPos);
        if (styleStart != std::string::npos) {
            size_t styleEnd = builtFile.find("</style>", styleStart);
            if (styleEnd != std::string::npos && startPos > styleStart && startPos < styleEnd) {
                insideStyle = true;
            }
        }

        if (insideScript || insideStyle) {
            endPos = startPos;  // skip this '{', move to next
            continue;
        }

        endPos = builtFile.find('}', startPos + 1);
        if (endPos == std::string::npos) {
            endPos = builtFile.size();
        }

        std::string keyword = builtFile.substr(startPos, endPos - startPos);
        std::string pathToInsert = root + keyword.substr(1);  // remove the '{' character

        std::string toInsert;

        if (fs::exists(pathToInsert)) {
            toInsert = loadFile(pathToInsert);
        }

        builtFile.replace(startPos, keyword.size() + 1, toInsert);
        // After replacement, endPos points to the end of inserted text
        endPos = startPos + toInsert.size() - 1;
    }
}

void HtmlBuilder::replaceTranslations(const std::string& language) {
    size_t startPos = 0;

    while ((startPos = builtFile.find('[', startPos)) != std::string::npos) {
        size_t endPos = builtFile.find(']', startPos + 1);

        if (endPos == std::string::npos) break;

        std::string keyword = builtFile.substr(startPos, endPos - startPos);
        std::string pathToInsert = root + keyword.substr(1);  // remove the '[' character

        // path_to_json:element
        std::string pathToJSON = pathToInsert.substr(0, pathToInsert.rfind(':'));
        std::string element = pathToInsert.substr(pathToInsert.rfind(':') + 1);

        std::string toInsert;

        if (fs::exists(pathToJSON)) {
            JSONParser* jsonParser = getJSONFromFile(pathToJSON);

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

void HtmlBuilder::replaceReferences(const std::string& language) {
    // Find if there are references in the file
    // href="/about_me" -> href="/en/about_me"
    // But skip CSS/JS files and /assets/ paths as they should not have language prefix

    // Process href="/" attributes
    size_t pos = 0;
    const std::string hrefPrefix = "href=\"/";

    while ((pos = builtFile.find(hrefPrefix, pos)) != std::string::npos) {
        size_t start = pos + hrefPrefix.length();  // position after href="/

        // Find the closing quote to get the full href value
        size_t endQuote = builtFile.find('"', start);
        if (endQuote != std::string::npos) {
            std::string href = builtFile.substr(start, endQuote - start);
            
            // Skip if it's a CSS file, /assets/ path, or already has language prefix
            if (href.find(".css") != std::string::npos || 
                href.compare(0, 7, "assets/") == 0 ||
                href.compare(0, language.length() + 1, language + "/") == 0) {
                pos = endQuote;
                continue;
            }
        }

        // Skip if it already starts with the language
        if (builtFile.compare(start, language.length() + 1, language + "/") != 0) {
            builtFile.insert(start, language + "/");
            pos = start + language.length() + 1;  // Move past inserted /lang/
        } else {
            pos = start + language.length() + 1;  // Already has /lang/, skip
        }
    }
    
    // Process src="/" attributes (for scripts, images, etc.)
    pos = 0;
    const std::string srcPrefix = "src=\"/";

    while ((pos = builtFile.find(srcPrefix, pos)) != std::string::npos) {
        size_t start = pos + srcPrefix.length();  // position after src="/

        // Find the closing quote to get the full src value
        size_t endQuote = builtFile.find('"', start);
        if (endQuote != std::string::npos) {
            std::string src = builtFile.substr(start, endQuote - start);
            
            // Skip if it's a JS file, /assets/ path, or already has language prefix
            if (src.find(".js") != std::string::npos || 
                src.compare(0, 7, "assets/") == 0 ||
                src.compare(0, language.length() + 1, language + "/") == 0) {
                pos = endQuote;
                continue;
            }
        }

        // Skip if it already starts with the language
        if (builtFile.compare(start, language.length() + 1, language + "/") != 0) {
            builtFile.insert(start, language + "/");
            pos = start + language.length() + 1;  // Move past inserted /lang/
        } else {
            pos = start + language.length() + 1;  // Already has /lang/, skip
        }
    }
}

std::string HtmlBuilder::getPageNameFromPath(const std::string& filePath) {
    // Extract the filename without extension from the path
    // e.g., "/root/html/en/about.html" -> "about"
    
    size_t lastSlash = filePath.find_last_of('/');
    std::string filename = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;
    
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        filename = filename.substr(0, lastDot);
    }
    
    return filename;
}

void HtmlBuilder::processAssetMerging(const std::string& pageName) {
    // Create AssetMerger and process the HTML
    AssetMerger merger(root, removeComments);
    MergeResult result = merger.processHtml(builtFile, pageName);
    
    // Update the HTML content with merged asset references
    builtFile = result.modifiedHtml;
    
    // Save merged CSS file if there are multiple CSS files to merge
    if (result.hasCss && !result.mergedCss.empty()) {
        std::string cssPath = result.cssSubdir.empty() ?
            root + "/assets/css/" + pageName + ".css" :
            root + "/assets/css/" + result.cssSubdir + "/" + pageName + ".css";
        FileManagement::saveFile(cssPath, result.mergedCss);
    }
    
    // Save merged JS file if there are multiple JS files to merge
    if (result.hasJs && !result.mergedJs.empty()) {
        std::string jsPath = result.jsSubdir.empty() ?
            root + "/assets/js/" + pageName + ".js" :
            root + "/assets/js/" + result.jsSubdir + "/" + pageName + ".js";
        FileManagement::saveFile(jsPath, result.mergedJs);
    }
}

void HtmlBuilder::ensureAbsoluteAssetPaths() {
    // Ensure all CSS href and JS src attributes have leading slashes
    // This makes them absolute paths that work from any page depth
    
    std::regex cssRegex(R"(href\s*=\s*["'](?!/)([^"':]+\.css)["'])");
    std::regex jsRegex(R"(src\s*=\s*["'](?!/)([^"':]+\.js)["'])");
    
    // Process CSS paths: href="base.css" -> href="/base.css"
    std::string result;
    std::smatch match;
    std::string::const_iterator searchStart(builtFile.cbegin());
    
    while (std::regex_search(searchStart, builtFile.cend(), match, cssRegex)) {
        result += match.prefix();
        
        // Add leading slash to the path
        std::string relativePath = match[1].str();
        result += "href=\"/" + relativePath + "\"";
        searchStart = match.suffix().first;
    }
    result += std::string(searchStart, builtFile.cend());
    builtFile = result;
    
    // Process JS paths: src="utils.js" -> src="/utils.js"
    result.clear();
    searchStart = builtFile.cbegin();
    
    while (std::regex_search(searchStart, builtFile.cend(), match, jsRegex)) {
        result += match.prefix();
        
        // Add leading slash to the path
        std::string relativePath = match[1].str();
        result += "src=\"/" + relativePath + "\"";
        searchStart = match.suffix().first;
    }
    result += std::string(searchStart, builtFile.cend());
    builtFile = result;
}

}  // namespace geruest
