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
#include <cstdio>
#include <filesystem>

namespace geruest {

namespace {

bool translationBracketInsideScript(const std::string& html, size_t bracketPos) {
    size_t scriptStart = html.rfind("<script", bracketPos);
    if (scriptStart == std::string::npos) {
        return false;
    }
    size_t scriptEnd = html.find("</script>", scriptStart);
    if (scriptEnd == std::string::npos) {
        return false;
    }
    return bracketPos > scriptStart && bracketPos < scriptEnd;
}

/// Escape for text substituted inside a double-quoted JavaScript string literal (e.g. "...[...]...").
std::string escapeTranslationForJsDoubleQuoted(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20U) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

}  // namespace

// Initialize static members
std::unordered_map<std::string, std::string> HtmlBuilder::_mergedAssetsCache;
std::mutex HtmlBuilder::_cacheMutex;

HtmlBuilder::HtmlBuilder(const std::string& inputPath, const ServerData& serverData)
    : ContentBuilder(inputPath, serverData) {
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
    if (language.empty() && !_serverData.getAvailableLanguages().empty()) {
        language = _serverData.getAvailableLanguages()[0];
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
    if (_serverData.getRemoveComments()) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_HTML);
    }

    // Check if template file was loaded
    if (builtFile.empty()) return;

    // Only process templates and save if using template system (not language-specific files)
    if (!useLanguageSpecificFile) {
        // Check if the target language is supported before processing and saving
        // Extract language from path: /root/html/XX/file.html
        bool languageSupported =
            _serverData.getAvailableLanguages().empty() ||
            std::find(_serverData.getAvailableLanguages().begin(), _serverData.getAvailableLanguages().end(), language) != _serverData.getAvailableLanguages().end();

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
        if (_serverData.getMergeAssets()) {
            std::string pageName = getPageNameFromPath(path);
            processAssetMerging(pageName);
        } else {
            // When merging is disabled, ensure all asset paths have leading slashes
            ensureAbsoluteAssetPaths();
        }

        // Process PNG/JPG to WebP conversion if enabled
        if (_serverData.getWebPConversion()) {
            processWebPConversion();
        }

        // Change references to the correct path
        replaceReferences(language);

        // Save the file only if NOT in dev mode
        // In dev mode, files are kept in memory only for faster iteration
        if (!_serverData.isDevMode()) {
            FileManagement::saveFile(path, builtFile);
            // Note: If save fails, content is still in builtFile for serving
        }
        // If in dev mode, content stays in builtFile for serving without disk writes
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
        std::string keywordPath = keyword.substr(1);  // remove the '[' character
        
        // Ensure the path starts with '/'
        if (!keywordPath.empty() && keywordPath[0] != '/') {
            keywordPath = "/" + keywordPath;
        }
        
        std::string pathToInsert = root + keywordPath;

        // path_to_json:element
        std::string pathToJSON = pathToInsert.substr(0, pathToInsert.rfind(':'));
        std::string element = pathToInsert.substr(pathToInsert.rfind(':') + 1);

        std::string toInsert;

        if (fs::exists(pathToJSON)) {
            auto jsonParser = getJSONFromFileSafe(pathToJSON);

            // Get the right language
            JSONParser languageArray = jsonParser->getObject(language);

            // If language doesn't exist, use English
            if (languageArray.getKeys().empty()) {
                languageArray = jsonParser->getObject("en");
            }

            // Get the element
            toInsert = languageArray.getString(element);
        }

        if (!toInsert.empty() && translationBracketInsideScript(builtFile, startPos)) {
            toInsert = escapeTranslationForJsDoubleQuoted(toInsert);
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
            
            // Skip if it's a CSS file or /assets/ path
            if (href.find(".css") != std::string::npos || 
                href.compare(0, 7, "assets/") == 0) {
                pos = endQuote;
                continue;
            }
            
            // Check if it already starts with any supported language
            bool hasLanguagePrefix = false;
            for (const auto& lang : _serverData.getAvailableLanguages()) {
                if (href.compare(0, lang.length() + 1, lang + "/") == 0 ||
                    href == lang) {  // Also check if href is exactly the language (e.g., href="/de")
                    hasLanguagePrefix = true;
                    break;
                }
            }
            
            if (hasLanguagePrefix) {
                pos = endQuote;
                continue;
            }
        }

        // Add language prefix if it doesn't have one already
        builtFile.insert(start, language + "/");
        pos = start + language.length() + 1;  // Move past inserted /lang/
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
            
            // Skip if it's a JS file, image file, or /assets/ path
            if (src.find(".js") != std::string::npos || 
                src.find(".png") != std::string::npos ||
                src.find(".jpg") != std::string::npos ||
                src.find(".jpeg") != std::string::npos ||
                src.find(".gif") != std::string::npos ||
                src.find(".svg") != std::string::npos ||
                src.find(".webp") != std::string::npos ||
                src.find(".ico") != std::string::npos ||
                src.compare(0, 7, "assets/") == 0) {
                pos = endQuote;
                continue;
            }
            
            // Check if it already starts with any supported language
            bool hasLanguagePrefix = false;
            for (const auto& lang : _serverData.getAvailableLanguages()) {
                if (src.compare(0, lang.length() + 1, lang + "/") == 0 ||
                    src == lang) {  // Also check if src is exactly the language
                    hasLanguagePrefix = true;
                    break;
                }
            }
            
            if (hasLanguagePrefix) {
                pos = endQuote;
                continue;
            }
        }

        // Add language prefix if it doesn't have one already
        builtFile.insert(start, language + "/");
        pos = start + language.length() + 1;  // Move past inserted /lang/
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
    AssetMerger merger(root, _serverData.getRemoveComments(), 
                      _serverData.getObfuscationExclusions());
    MergeResult result = merger.processHtml(builtFile, pageName);
    
    // Update the HTML content with merged asset references
    builtFile = result.modifiedHtml;
    
    // In dev mode: Store merged assets in memory cache instead of saving to disk
    // In production: Save merged assets to disk for performance
    if (_serverData.isDevMode()) {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        
        // Store merged CSS in cache
        if (result.hasCss && !result.mergedCss.empty()) {
            std::string cssPath = result.cssSubdir.empty() ?
                "/assets/css/" + pageName + ".css" :
                "/assets/css/" + result.cssSubdir + "/" + pageName + ".css";
            _mergedAssetsCache[cssPath] = result.mergedCss;
        }
        
        // Store merged JS in cache
        if (result.hasJs && !result.mergedJs.empty()) {
            std::string jsPath = result.jsSubdir.empty() ?
                "/assets/js/" + pageName + ".js" :
                "/assets/js/" + result.jsSubdir + "/" + pageName + ".js";
            _mergedAssetsCache[jsPath] = result.mergedJs;
        }
    } else {
        // Production mode: Save to disk
        if (result.hasCss && !result.mergedCss.empty()) {
            std::string cssPath = result.cssSubdir.empty() ?
                root + "/assets/css/" + pageName + ".css" :
                root + "/assets/css/" + result.cssSubdir + "/" + pageName + ".css";
            FileManagement::saveFile(cssPath, result.mergedCss);
        }
        
        if (result.hasJs && !result.mergedJs.empty()) {
            std::string jsPath = result.jsSubdir.empty() ?
                root + "/assets/js/" + pageName + ".js" :
                root + "/assets/js/" + result.jsSubdir + "/" + pageName + ".js";
            FileManagement::saveFile(jsPath, result.mergedJs);
        }
    }
}

void HtmlBuilder::ensureAbsoluteAssetPaths() {
    // Ensure all CSS href, JS src, and image src attributes have leading slashes
    // This makes them absolute paths that work from any page depth
    
    std::regex cssRegex(R"(href\s*=\s*["'](?!/)([^"':]+\.css)["'])");
    std::regex jsRegex(R"(src\s*=\s*["'](?!/)([^"':]+\.js)["'])");
    std::regex imgRegex(R"(src\s*=\s*["'](?!/)([^"':]+\.(?:png|jpg|jpeg|gif|svg|webp|ico))["'])");
    
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
    
    // Process image paths: src="icon.svg" -> src="/icon.svg"
    result.clear();
    searchStart = builtFile.cbegin();
    
    while (std::regex_search(searchStart, builtFile.cend(), match, imgRegex)) {
        result += match.prefix();
        
        // Add leading slash to the path
        std::string relativePath = match[1].str();
        result += "src=\"/" + relativePath + "\"";
        searchStart = match.suffix().first;
    }
    result += std::string(searchStart, builtFile.cend());
    builtFile = result;
}

std::string HtmlBuilder::getMergedAssetFromCache(const std::string& path) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    auto it = _mergedAssetsCache.find(path);
    if (it != _mergedAssetsCache.end()) {
        return it->second;
    }
    return "";
}

bool HtmlBuilder::hasMergedAssetInCache(const std::string& path) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    return _mergedAssetsCache.find(path) != _mergedAssetsCache.end();
}

std::shared_ptr<const std::vector<uint8_t>> HtmlBuilder::getWebPFromCache(const std::string& path) {
    return WebPConverter::getFromCache(path);
}

bool HtmlBuilder::hasWebPInCache(const std::string& path) {
    return WebPConverter::hasInCache(path);
}

void HtmlBuilder::processWebPConversion() {
    // Extract all image paths from HTML
    // Images are expected to be in assets/images/ directory (standard Geruest path)
    std::vector<std::string> imagePaths = WebPConverter::extractImagePathsFromHtml(builtFile);
    
    if (imagePaths.empty()) {
        return;
    }
    
    // Ensure root has trailing slash for path concatenation
    std::string rootPath = root;
    if (!rootPath.empty() && rootPath.back() != '/' && rootPath.back() != '\\') {
        rootPath += '/';
    }
    
    // Standard image base path in Geruest
    const std::string imageBasePath = "assets/images/";
    
    // Process each image
    for (const auto& relativePath : imagePaths) {
        // Build full path to source image
        // relativePath may or may not have leading slash, handle both cases
        std::string cleanRelativePath = relativePath;
        if (!cleanRelativePath.empty() && (cleanRelativePath[0] == '/' || cleanRelativePath[0] == '\\')) {
            cleanRelativePath = cleanRelativePath.substr(1);
        }
        
        // Prepend assets/images/ to find the actual file
        // HTML has "Home/image.jpg" but file is at "assets/images/Home/image.jpg"
        std::string fullSourcePath = rootPath + imageBasePath + cleanRelativePath;
        
        // Determine output path (change extension to .webp)
        std::string webpRelativePath = cleanRelativePath;
        size_t dotPos = webpRelativePath.rfind('.');
        if (dotPos != std::string::npos) {
            webpRelativePath = webpRelativePath.substr(0, dotPos) + ".webp";
        }
        std::string fullWebPPath = rootPath + imageBasePath + webpRelativePath;
        
        // Convert image to WebP
        bool success = false;
        if (_serverData.isDevMode()) {
            // In dev mode: only convert if not already cached
            if (WebPConverter::hasInCache(fullWebPPath)) {
                success = true;
            } else {
                success = WebPConverter::convertImage(fullSourcePath, fullWebPPath, true, _serverData.getWebPQuality());
            }
        } else {
            // Production mode: check if WebP already exists and is newer than source
            if (std::filesystem::exists(fullWebPPath)) {
                auto sourceTime = std::filesystem::last_write_time(fullSourcePath);
                auto webpTime = std::filesystem::last_write_time(fullWebPPath);
                if (webpTime >= sourceTime) {
                    // WebP is up-to-date, skip conversion
                    success = true;
                }
            }
            
            if (!success) {
                // Convert and save to disk
                success = WebPConverter::convertImage(fullSourcePath, fullWebPPath, false, _serverData.getWebPQuality());
            }
        }
        
        // Note: We don't replace references in builtFile here because
        // replaceImageReferencesWithWebP will be called if conversion was successful
    }
    
    // Replace all image references in HTML with .webp extensions
    // Keep the same path, just change the extension
    builtFile = WebPConverter::replaceImageReferencesWithWebP(builtFile);
}

}  // namespace geruest
