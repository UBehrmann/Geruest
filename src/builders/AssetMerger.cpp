/**
 * @file AssetMerger.cpp
 * @date 19.12.2025
 *
 * @author Urs Behrmann
 *
 * @brief Implementation of the AssetMerger class for automatic CSS/JS merging.
 */

#include "AssetMerger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace geruest {

namespace {

/** True iff code[pos] is '/' and the immediately preceding run of '\\' has odd length (escaped slash). */
bool isEscapedSlashBefore(const std::string& code, size_t pos) {
    if (pos == 0 || code[pos] != '/') {
        return false;
    }
    size_t backslashes = 0;
    for (size_t k = pos; k > 0 && code[k - 1] == '\\'; --k) {
        ++backslashes;
    }
    return (backslashes % 2U) == 1U;
}

/** Close brace for '${' inside template interpolation (matches JSObfuscator / Acorn-style skipping). */
size_t templateInterpolationCloseAm(const std::string& s, size_t innerBegin) {
    size_t pos = innerBegin;
    int depth = 1;
    bool inDq = false;
    bool inSq = false;
    bool inBt = false;

    while (pos < s.size()) {
        const char c = s[pos];
        if (!inDq && !inSq && !inBt) {
            if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '/') {
                pos += 2;
                while (pos < s.size() && s[pos] != '\n') {
                    ++pos;
                }
                continue;
            }
            if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '*') {
                pos += 2;
                while (pos + 1 < s.size() && !(s[pos] == '*' && s[pos + 1] == '/')) {
                    ++pos;
                }
                pos = (pos + 1 < s.size()) ? pos + 2 : s.size();
                continue;
            }
            if (c == '"') {
                inDq = true;
                ++pos;
                continue;
            }
            if (c == '\'') {
                inSq = true;
                ++pos;
                continue;
            }
            if (c == '`') {
                inBt = true;
                ++pos;
                continue;
            }
            if (c == '{') {
                ++depth;
                ++pos;
                continue;
            }
            if (c == '}') {
                --depth;
                if (depth == 0) {
                    return pos;
                }
                ++pos;
                continue;
            }
            ++pos;
            continue;
        }
        if (inDq) {
            if (c == '\\' && pos + 1 < s.size()) {
                pos += 2;
                continue;
            }
            if (c == '"') {
                inDq = false;
            }
            ++pos;
            continue;
        }
        if (inSq) {
            if (c == '\\' && pos + 1 < s.size()) {
                pos += 2;
                continue;
            }
            if (c == '\'') {
                inSq = false;
            }
            ++pos;
            continue;
        }
        if (inBt) {
            if (c == '\\' && pos + 1 < s.size()) {
                pos += 2;
                continue;
            }
            if (c == '`') {
                inBt = false;
                ++pos;
                continue;
            }
            if (c == '$' && pos + 1 < s.size() && s[pos + 1] == '{') {
                pos += 2;
                const size_t closeInner = templateInterpolationCloseAm(s, pos);
                if (closeInner == std::string::npos) {
                    return std::string::npos;
                }
                pos = closeInner + 1;
                continue;
            }
            ++pos;
            continue;
        }
    }
    return std::string::npos;
}

/// From opening '`', return index past closing '`' (handles `${...}` and nested templates).
size_t skipTemplateLiteralAm(const std::string& code, size_t openTick) {
    if (openTick >= code.size() || code[openTick] != '`') {
        return openTick + 1;
    }
    size_t pos = openTick + 1;
    const size_t n = code.size();
    while (pos < n) {
        if (code[pos] == '\\' && pos + 1 < n) {
            pos += 2;
            continue;
        }
        if (code[pos] == '`') {
            return pos + 1;
        }
        if (code[pos] == '$' && pos + 1 < n && code[pos + 1] == '{') {
            pos += 2;
            const size_t close = templateInterpolationCloseAm(code, pos);
            if (close == std::string::npos) {
                return n;
            }
            pos = close + 1;
            continue;
        }
        ++pos;
    }
    return n;
}

static void skipJsStringOrTemplate(const std::string& s, size_t& pos) {
    if (pos >= s.size()) {
        return;
    }
    if (s[pos] == '`') {
        pos = skipTemplateLiteralAm(s, pos);
        return;
    }
    if (s[pos] == '"' || s[pos] == '\'') {
        const char quote = s[pos];
        ++pos;
        while (pos < s.size()) {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos += 2;
            } else if (s[pos] == quote) {
                ++pos;
                break;
            } else {
                ++pos;
            }
        }
    }
}

}  // namespace

AssetMerger::AssetMerger(const std::string& serverRoot, bool removeComments,
                         const std::vector<std::string>& exclusions)
    : _serverRoot(serverRoot), _removeComments(removeComments), _exclusions(exclusions) {
}

std::string AssetMerger::loadFile(const std::string& filePath) {
    std::ifstream fileStream(filePath, std::ios::binary | std::ios::ate);
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

bool AssetMerger::isExternalUrl(const std::string& url) {
    return url.find("http://") == 0 || url.find("https://") == 0 || url.find("//") == 0;
}

std::string AssetMerger::removeCssComments(const std::string& content) {
    std::string result = content;
    size_t start = 0;
    while ((start = result.find("/*", start)) != std::string::npos) {
        size_t end = result.find("*/", start + 2);
        if (end == std::string::npos) break;
        result.erase(start, end - start + 2);
    }
    return result;
}

std::string AssetMerger::removeJsComments(const std::string& content) {
    std::string result = content;
    
    // Remove /* ... */ comments (handling string literals)
    size_t pos = 0;
    while (pos < result.length()) {
        if (result[pos] == '"' || result[pos] == '\'' || result[pos] == '`') {
            skipJsStringOrTemplate(result, pos);
            continue;
        }

        // Skip // line comments first. Otherwise a line like
        //   // ... path/translations/*.json
        // contains "/*" inside the comment text; treating it as a block opener pairs with a
        // later real "*/" (e.g. JSDoc) and erases the entire region — catastrophic.
        if (pos < result.length() - 1 && result[pos] == '/' && result[pos + 1] == '/' &&
            !isEscapedSlashBefore(result, pos)) {
            size_t lineEnd = result.find('\n', pos);
            if (lineEnd == std::string::npos) {
                result.erase(pos);
                break;
            }
            result.erase(pos, lineEnd - pos);
            continue;
        }

        // Look for /* comment outside of strings
        if (pos < result.length() - 1 && result[pos] == '/' && result[pos + 1] == '*') {
            size_t end = result.find("*/", pos + 2);
            if (end == std::string::npos) break;
            result.erase(pos, end - pos + 2);
            continue;
        }
        
        pos++;
    }

    return result;
}

bool AssetMerger::isExcluded(const std::string& filename) const {
    // Extract just the filename from path
    size_t lastSlash = filename.find_last_of("/\\");
    std::string basename = (lastSlash != std::string::npos) ? filename.substr(lastSlash + 1) : filename;
    
    for (const auto& excluded : _exclusions) {
        if (basename == excluded) {
            return true;
        }
    }
    return false;
}

std::string AssetMerger::resolveAssetPath(const std::string& href, const std::string& assetType) {
    std::string cleanHref = href;
    
    // Remove leading slash if present
    if (!cleanHref.empty() && cleanHref[0] == '/') {
        cleanHref = cleanHref.substr(1);
    }
    
    // Determine the asset directory
    std::string assetDir = (assetType == "css") ? "/assets/css/" : "/assets/js/";
    
    // Check if the href already includes the asset path
    if (cleanHref.find("assets/css/") == 0 || cleanHref.find("assets/js/") == 0) {
        return _serverRoot + "/" + cleanHref;
    }
    
    // Otherwise, assume it's relative to the asset directory
    return _serverRoot + assetDir + cleanHref;
}

std::vector<AssetReference> AssetMerger::extractCssReferences(const std::string& htmlContent) {
    std::vector<AssetReference> references;
    
    // Pattern to match <link rel="stylesheet" href="...">
    // Handles various attribute orderings and quote styles
    std::regex linkPattern(
        R"(<link\s+[^>]*rel\s*=\s*["']stylesheet["'][^>]*href\s*=\s*["']([^"']+)["'][^>]*>|)"
        R"(<link\s+[^>]*href\s*=\s*["']([^"']+)["'][^>]*rel\s*=\s*["']stylesheet["'][^>]*>)",
        std::regex::icase
    );
    
    std::string::const_iterator searchStart(htmlContent.cbegin());
    std::smatch match;
    
    while (std::regex_search(searchStart, htmlContent.cend(), match, linkPattern)) {
        AssetReference ref;
        
        // Get the href from whichever group matched
        ref.href = match[1].matched ? match[1].str() : match[2].str();
        ref.startPos = static_cast<size_t>(match.position()) + 
                       static_cast<size_t>(std::distance(htmlContent.cbegin(), searchStart));
        ref.endPos = ref.startPos + match.length();
        ref.isExternal = isExternalUrl(ref.href);
        
        references.push_back(ref);
        searchStart = match.suffix().first;
    }
    
    return references;
}

std::vector<AssetReference> AssetMerger::extractJsReferences(const std::string& htmlContent) {
    std::vector<AssetReference> references;
    
    // Pattern to match <script src="..."></script> or <script src="..."/>
    // Only matches external scripts (with src attribute), not inline scripts
    std::regex scriptPattern(
        R"(<script\s+[^>]*src\s*=\s*["']([^"']+)["'][^>]*>\s*</script>|)"
        R"(<script\s+[^>]*src\s*=\s*["']([^"']+)["'][^>]*/>)",
        std::regex::icase
    );
    
    std::string::const_iterator searchStart(htmlContent.cbegin());
    std::smatch match;
    
    while (std::regex_search(searchStart, htmlContent.cend(), match, scriptPattern)) {
        AssetReference ref;
        
        // Get the src from whichever group matched
        ref.href = match[1].matched ? match[1].str() : match[2].str();
        ref.startPos = static_cast<size_t>(match.position()) + 
                       static_cast<size_t>(std::distance(htmlContent.cbegin(), searchStart));
        ref.endPos = ref.startPos + match.length();
        ref.isExternal = isExternalUrl(ref.href);
        
        references.push_back(ref);
        searchStart = match.suffix().first;
    }
    
    return references;
}

std::string AssetMerger::mergeCssFiles(const std::vector<std::string>& cssFiles) {
    std::string merged;
    
    for (const auto& filePath : cssFiles) {
        std::string content = loadFile(filePath);
        if (!content.empty()) {
            // Add a comment header for each file (useful for debugging)
            if (!_removeComments) {
                merged += "/* === " + fs::path(filePath).filename().string() + " === */\n";
            }
            
            if (_removeComments) {
                content = removeCssComments(content);
            }
            
            merged += content + "\n\n";
        }
    }
    
    return merged;
}

std::string AssetMerger::mergeJsFiles(const std::vector<std::string>& jsFiles) {
    std::string merged;
    
    for (const auto& filePath : jsFiles) {
        std::string content = loadFile(filePath);
        if (!content.empty()) {
            // Add a comment header for each file (useful for debugging)
            if (!_removeComments) {
                merged += "// === " + fs::path(filePath).filename().string() + " ===\n";
            }
            
            if (_removeComments) {
                content = removeJsComments(content);
            }
            
            // Simply concatenate files without wrapper
            merged += content + "\n\n";
        }
    }
    
    return merged;
}

namespace {

thread_local std::string g_tlExpectedMergedCanonKey;
thread_local fs::path g_tlExpectedMergedCanonVal;
thread_local bool g_tlExpectedMergedCanonOk = false;

bool cachedWeaklyCanonicalExpectedOut(const fs::path& expectedMerged, fs::path& out) {
    std::error_code ec;
    const std::string key = expectedMerged.string();
    if (g_tlExpectedMergedCanonOk && g_tlExpectedMergedCanonKey == key) {
        out = g_tlExpectedMergedCanonVal;
        return true;
    }
    out = fs::weakly_canonical(expectedMerged, ec);
    if (ec) {
        g_tlExpectedMergedCanonOk = false;
        return false;
    }
    g_tlExpectedMergedCanonKey = key;
    g_tlExpectedMergedCanonVal = out;
    g_tlExpectedMergedCanonOk = true;
    return true;
}

std::string readWholeFileForMerge(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    const auto fileSize = in.tellg();
    if (fileSize <= 0) {
        return {};
    }
    std::string content(static_cast<size_t>(fileSize), '\0');
    in.seekg(0);
    in.read(&content[0], fileSize);
    return content;
}

/**
 * Merged page bundles are often written to the same path as a source <script src>
 * (e.g. assets/js/checkUserBooks.js). The next merge must not load that file as a
 * fresh segment when it already contains the full bundle (would duplicate main.js, etc.).
 */
std::string mergeJsFilesResolvingBundleWriteback(AssetMerger& merger,
                                                 const std::vector<std::string>& localJsFiles,
                                                 const std::string& serverRoot,
                                                 const std::string& pageName,
                                                 const std::string& jsSubdir) {
    fs::path expectedMerged = fs::path(serverRoot) / "assets" / "js" / (pageName + ".js");
    if (!jsSubdir.empty()) {
        expectedMerged = fs::path(serverRoot) / "assets" / "js" / jsSubdir / (pageName + ".js");
    }

    fs::path canonOut;
    if (!cachedWeaklyCanonicalExpectedOut(expectedMerged, canonOut)) {
        return merger.mergeJsFiles(localJsFiles);
    }

    std::unordered_map<std::string, fs::path> canonPerFile;
    canonPerFile.reserve(localJsFiles.size());
    std::error_code ec;

    int conflictIndex = -1;
    for (size_t i = 0; i < localJsFiles.size(); ++i) {
        const std::string& absPath = localJsFiles[i];
        fs::path canonFile;
        auto found = canonPerFile.find(absPath);
        if (found != canonPerFile.end()) {
            canonFile = found->second;
        } else {
            ec.clear();
            canonFile = fs::weakly_canonical(absPath, ec);
            if (ec) {
                continue;
            }
            canonPerFile.emplace(absPath, canonFile);
        }
        if (canonFile == canonOut) {
            conflictIndex = static_cast<int>(i);
            break;
        }
    }

    if (conflictIndex < 0) {
        return merger.mergeJsFiles(localJsFiles);
    }

    const std::vector<std::string> prefixPaths(localJsFiles.begin(),
                                               localJsFiles.begin() + conflictIndex);
    const std::string prefixMerged =
        prefixPaths.empty() ? std::string() : merger.mergeJsFiles(prefixPaths);
    const std::string raw = readWholeFileForMerge(localJsFiles[static_cast<size_t>(conflictIndex)]);

    if (!prefixMerged.empty() && raw.size() >= prefixMerged.size() &&
        raw.compare(0, prefixMerged.size(), prefixMerged) == 0) {
        return raw;
    }
    if (prefixPaths.empty()) {
        return merger.mergeJsFiles({localJsFiles[static_cast<size_t>(conflictIndex)]});
    }
    return prefixMerged + merger.mergeJsFiles({localJsFiles[static_cast<size_t>(conflictIndex)]});
}

}  // namespace

JsMergeDiscovery AssetMerger::discoverJsMergeInputs(const std::string& htmlContent) {
    JsMergeDiscovery d;
    auto jsRefs = extractJsReferences(htmlContent);
    for (const auto& ref : jsRefs) {
        if (ref.isExternal) {
            continue;
        }
        if (isExcluded(ref.href)) {
            continue;
        }
        std::string filePath = resolveAssetPath(ref.href, "js");
        if (fs::exists(filePath)) {
            d.localJsAbsolutePaths.push_back(std::move(filePath));
            d.jsHrefs.push_back(ref.href);
            if (d.jsSubdir.empty()) {
                size_t lastSlash = ref.href.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    d.jsSubdir = ref.href.substr(0, lastSlash);
                }
            }
        }
    }
    d.hasJs = d.localJsAbsolutePaths.size() >= 1;
    return d;
}

MergeResult AssetMerger::processHtml(const std::string& htmlContent, const std::string& pageName) {
    MergeResult result;
    result.modifiedHtml = htmlContent;
    result.hasCss = false;
    result.hasJs = false;
    
    // Extract CSS references
    auto cssRefs = extractCssReferences(htmlContent);
    
    // Collect local CSS files (skip external URLs and excluded files)
    std::vector<std::string> localCssFiles;
    for (const auto& ref : cssRefs) {
        if (!ref.isExternal) {
            // Check if file is excluded
            if (isExcluded(ref.href)) {
                continue;  // Skip excluded files
            }
            
            std::string filePath = resolveAssetPath(ref.href, "css");
            if (fs::exists(filePath)) {
                localCssFiles.push_back(filePath);
                result.cssFiles.push_back(ref.href);
                
                // Extract subdirectory from first CSS file (full nested path)
                if (result.cssSubdir.empty()) {
                    // href format: "subdir/subdir2/file.css" or "file.css"
                    // Extract everything before the filename
                    size_t lastSlash = ref.href.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        result.cssSubdir = ref.href.substr(0, lastSlash);
                    }
                }
            }
        }
    }

    JsMergeDiscovery jsDisc = discoverJsMergeInputs(htmlContent);
    result.jsFiles = std::move(jsDisc.jsHrefs);
    result.jsSubdir = std::move(jsDisc.jsSubdir);
    std::vector<std::string> localJsFiles = std::move(jsDisc.localJsAbsolutePaths);

    auto jsRefs = extractJsReferences(htmlContent);
    
    // Determine if we should merge (always merge if there's at least 1 file)
    bool shouldMergeCss = localCssFiles.size() >= 1;
    bool shouldMergeJs = localJsFiles.size() >= 1;
    
    // Build new HTML, replacing asset tags as we go
    std::string newHtml;
    size_t lastPos = 0;
    size_t currentCssIndex = 0;
    size_t currentJsIndex = 0;
    bool mergedCssInserted = false;
    bool mergedJsInserted = false;
    
    // Create merged content if needed
    if (shouldMergeCss) {
        result.mergedCss = mergeCssFiles(localCssFiles);
        result.hasCss = true;
    }
    
    if (shouldMergeJs) {
        result.mergedJs = mergeJsFilesResolvingBundleWriteback(*this, localJsFiles, _serverRoot,
                                                               pageName, result.jsSubdir);
        result.hasJs = true;
    }
    
    // Process all asset references in order
    size_t nextCssPos = 0;
    size_t nextJsPos = 0;
    
    while (lastPos < htmlContent.size()) {
        // Find next CSS tag position
        if (shouldMergeCss && currentCssIndex < cssRefs.size()) {
            while (currentCssIndex < cssRefs.size() && cssRefs[currentCssIndex].isExternal) {
                currentCssIndex++;
            }
            nextCssPos = (currentCssIndex < cssRefs.size()) ? cssRefs[currentCssIndex].startPos : std::string::npos;
        } else {
            nextCssPos = std::string::npos;
        }
        
        // Find next JS tag position
        if (shouldMergeJs && currentJsIndex < jsRefs.size()) {
            while (currentJsIndex < jsRefs.size() && jsRefs[currentJsIndex].isExternal) {
                currentJsIndex++;
            }
            nextJsPos = (currentJsIndex < jsRefs.size()) ? jsRefs[currentJsIndex].startPos : std::string::npos;
        } else {
            nextJsPos = std::string::npos;
        }
        
        // Determine which comes first
        size_t nextPos = std::string::npos;
        bool isCss = false;
        
        if (nextCssPos != std::string::npos && nextJsPos != std::string::npos) {
            if (nextCssPos < nextJsPos) {
                nextPos = nextCssPos;
                isCss = true;
            } else {
                nextPos = nextJsPos;
                isCss = false;
            }
        } else if (nextCssPos != std::string::npos) {
            nextPos = nextCssPos;
            isCss = true;
        } else if (nextJsPos != std::string::npos) {
            nextPos = nextJsPos;
            isCss = false;
        }
        
        // If no more tags, copy rest and break
        if (nextPos == std::string::npos) {
            newHtml += htmlContent.substr(lastPos);
            break;
        }
        
        // Copy everything before this tag
        newHtml += htmlContent.substr(lastPos, nextPos - lastPos);
        
        // Handle the tag
        if (isCss) {
            size_t tagEnd = cssRefs[currentCssIndex].endPos;
            
            // First CSS tag: insert merged CSS link, skip all others
            if (!mergedCssInserted) {
                std::string cssHref = result.cssSubdir.empty() ? 
                    "/" + pageName + ".css" : 
                    "/" + result.cssSubdir + "/" + pageName + ".css";
                std::string mergedCssLink = "<link rel=\"stylesheet\" href=\"" + cssHref + "\">";
                newHtml += mergedCssLink;
                mergedCssInserted = true;
                // Skip to end of tag
                lastPos = tagEnd;
            } else {
                // Skip this tag AND the trailing newline if present
                lastPos = tagEnd;
                if (lastPos < htmlContent.size() && htmlContent[lastPos] == '\n') {
                    lastPos++;
                } else if (lastPos < htmlContent.size() - 1 && 
                          htmlContent[lastPos] == '\r' && htmlContent[lastPos + 1] == '\n') {
                    lastPos += 2;
                }
            }
            currentCssIndex++;
        } else {
            size_t tagEnd = jsRefs[currentJsIndex].endPos;
            
            // First JS tag: insert merged JS script, skip all others
            if (!mergedJsInserted) {
                std::string jsHref = result.jsSubdir.empty() ? 
                    "/" + pageName + ".js" : 
                    "/" + result.jsSubdir + "/" + pageName + ".js";
                std::string mergedJsScript = "<script src=\"" + jsHref + "\"></script>";
                newHtml += mergedJsScript;
                mergedJsInserted = true;
                // Skip to end of tag
                lastPos = tagEnd;
            } else {
                // Skip this tag AND the trailing newline if present
                lastPos = tagEnd;
                if (lastPos < htmlContent.size() && htmlContent[lastPos] == '\n') {
                    lastPos++;
                } else if (lastPos < htmlContent.size() - 1 && 
                          htmlContent[lastPos] == '\r' && htmlContent[lastPos + 1] == '\n') {
                    lastPos += 2;
                }
            }
            currentJsIndex++;
        }
    }
    
    result.modifiedHtml = newHtml;
    
    return result;
}

}  // namespace geruest
