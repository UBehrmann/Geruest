/**
 * @file JSBuilder.cpp
 * @date on: 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * 
 * When mergeAssets=false: Serves individual JS files as-is.
 * When mergeAssets=true: Serves the same per-page merged bundle as HTMLBuilder/AssetMerger.
 * JSBuilder rebuilds that bundle from the page HTML template when serving the merged
 * script URL so obfuscation always sees one compilation unit even if the .js file was
 * requested before any HTML response (dev cache miss or production cold start).
 * In production, if the on-disk merged bundle is at least as new as the template and
 * source scripts, that file is reused without re-running merge.
 *
 * Obfuscation compilation unit: The string passed to JSObfuscator is always the exact
 * bytes served for that script URL (per-file source or the merged bundle). Rename maps
 * are computed once per obfuscate() call — use merged output as the input when mergeAssets
 * is true so cross-script bindings share one scope pass. Per-file obfuscation caches for
 * later concatenation are unsupported and would desynchronize names.
 *
 * Obfuscation flow:
 * 1. Check if file is excluded -> serve original
 * 2. Check if dev mode or level=0 -> serve original (possibly merged)
 * 3. Check if cached obfuscated file exists and is valid -> serve cached
 * 4. Otherwise: obfuscate, cache to disk, and serve
 */

#include "JSBuilder.hpp"
#include "AssetMerger.hpp"
#include "JSObfuscator.hpp"
#include "../FileManagement/FileManagement.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace geruest {

namespace {

namespace fs = std::filesystem;

struct TemplateMergedJsInfo {
    std::string mergedJs;
    std::string htmlTemplatePath;
    std::vector<std::string> mergedJsHrefs;
    /** If true, mergedJs is empty and builtFile already came from loadFile (on-disk bundle). */
    bool diskReuse = false;
};

bool tlWeaklyCanonicalAbsPath(const std::string& absPath, fs::path& out) {
    thread_local std::string key;
    thread_local fs::path val;
    thread_local bool ok = false;
    std::error_code ec;
    if (ok && key == absPath) {
        out = val;
        return true;
    }
    out = fs::weakly_canonical(absPath, ec);
    if (ec) {
        ok = false;
        return false;
    }
    key = absPath;
    val = out;
    ok = true;
    return true;
}

bool tlWeaklyCanonicalExpectedMergedPath(const fs::path& expectedMerged, fs::path& out) {
    thread_local std::string key;
    thread_local fs::path val;
    thread_local bool ok = false;
    std::error_code ec;
    const std::string cand = expectedMerged.string();
    if (ok && key == cand) {
        out = val;
        return true;
    }
    out = fs::weakly_canonical(expectedMerged, ec);
    if (ec) {
        ok = false;
        return false;
    }
    key = cand;
    val = out;
    ok = true;
    return true;
}

std::string readEntireFile(const std::string& filePath) {
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

std::string findBestPageTemplateHtml(const std::string& htmlDir, const std::string& pageName) {
    namespace fs = std::filesystem;
    const std::string target = pageName + ".html";
    const std::string direct = htmlDir + "/" + target;
    if (fs::exists(direct)) {
        return direct;
    }

    std::vector<std::string> matches;
    std::error_code ec;
    const fs::path rootPath(htmlDir);
    for (const auto& entry : fs::recursive_directory_iterator(htmlDir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename() == target) {
            matches.push_back(entry.path().string());
        }
    }
    if (matches.empty()) {
        return {};
    }

    auto best = matches.begin();
    size_t bestDepth = std::numeric_limits<size_t>::max();
    for (auto it = matches.begin(); it != matches.end(); ++it) {
        ec.clear();
        fs::path rel = fs::relative(*it, rootPath, ec);
        if (ec) {
            continue;
        }
        size_t depth = static_cast<size_t>(std::distance(rel.begin(), rel.end()));
        if (depth < bestDepth) {
            bestDepth = depth;
            best = it;
        } else if (depth == bestDepth && *it < *best) {
            best = it;
        }
    }
    return *best;
}

std::optional<TemplateMergedJsInfo> tryTemplateMergedJs(const std::string& jsAbsPath,
                                                        const ServerData& serverData) {
    namespace fs = std::filesystem;
    if (!serverData.getMergeAssets()) {
        return std::nullopt;
    }

    const std::string& root = serverData.getRoot();
    fs::path jsPath(jsAbsPath);
    const std::string pageStem = jsPath.stem().string();
    if (pageStem.empty()) {
        return std::nullopt;
    }

    const std::string htmlDir = root + "/html";
    if (!fs::is_directory(htmlDir)) {
        return std::nullopt;
    }

    const std::string templatePath = findBestPageTemplateHtml(htmlDir, pageStem);
    if (templatePath.empty()) {
        return std::nullopt;
    }

    const std::string htmlContent = readEntireFile(templatePath);
    if (htmlContent.empty()) {
        return std::nullopt;
    }

    AssetMerger merger(root, serverData.getRemoveComments(), serverData.getObfuscationExclusions());
    MergeResult mergeResult = merger.processHtml(htmlContent, pageStem);

    if (!mergeResult.hasJs || mergeResult.mergedJs.empty()) {
        return std::nullopt;
    }

    fs::path expectedMerged = fs::path(root) / "assets" / "js" / (pageStem + ".js");
    if (!mergeResult.jsSubdir.empty()) {
        expectedMerged = fs::path(root) / "assets" / "js" / mergeResult.jsSubdir / (pageStem + ".js");
    }

    fs::path canonJs;
    if (!tlWeaklyCanonicalAbsPath(jsAbsPath, canonJs)) {
        return std::nullopt;
    }
    fs::path canonExpected;
    if (!tlWeaklyCanonicalExpectedMergedPath(expectedMerged, canonExpected)) {
        return std::nullopt;
    }
    if (canonJs != canonExpected) {
        return std::nullopt;
    }

    TemplateMergedJsInfo info;
    info.mergedJs = std::move(mergeResult.mergedJs);
    info.htmlTemplatePath = templatePath;
    info.mergedJsHrefs = std::move(mergeResult.jsFiles);
    info.diskReuse = false;
    return info;
}

/**
 * Production: if merged bundle on disk is not older than the HTML template and source scripts,
 * skip processHtml/merge (builtFile already loaded in ContentBuilder).
 */
std::optional<TemplateMergedJsInfo> tryReuseOnDiskMergedBundle(const std::string& jsAbsPath,
                                                               const ServerData& serverData,
                                                               const std::string& loadedBundle) {
    namespace fs = std::filesystem;
    if (serverData.isDevMode() || !serverData.getMergeAssets() || loadedBundle.empty()) {
        return std::nullopt;
    }

    const std::string& root = serverData.getRoot();
    fs::path            jsPath(jsAbsPath);
    const std::string   pageStem = jsPath.stem().string();
    if (pageStem.empty()) {
        return std::nullopt;
    }

    const std::string htmlDir = root + "/html";
    if (!fs::is_directory(htmlDir)) {
        return std::nullopt;
    }

    const std::string templatePath = findBestPageTemplateHtml(htmlDir, pageStem);
    if (templatePath.empty()) {
        return std::nullopt;
    }

    const std::string htmlContent = readEntireFile(templatePath);
    if (htmlContent.empty()) {
        return std::nullopt;
    }

    AssetMerger merger(root, serverData.getRemoveComments(), serverData.getObfuscationExclusions());
    JsMergeDiscovery disc = merger.discoverJsMergeInputs(htmlContent);
    if (!disc.hasJs) {
        return std::nullopt;
    }

    fs::path expectedMerged = fs::path(root) / "assets" / "js" / (pageStem + ".js");
    if (!disc.jsSubdir.empty()) {
        expectedMerged = fs::path(root) / "assets" / "js" / disc.jsSubdir / (pageStem + ".js");
    }

    fs::path canonJs;
    if (!tlWeaklyCanonicalAbsPath(jsAbsPath, canonJs)) {
        return std::nullopt;
    }
    fs::path canonExpected;
    if (!tlWeaklyCanonicalExpectedMergedPath(expectedMerged, canonExpected)) {
        return std::nullopt;
    }
    if (canonJs != canonExpected) {
        return std::nullopt;
    }

    std::error_code ec;
    try {
        const auto bundleTime = fs::last_write_time(jsAbsPath, ec);
        if (ec) {
            return std::nullopt;
        }
        auto newestInput = fs::last_write_time(templatePath);
        for (const auto& href : disc.jsHrefs) {
            const std::string scriptPath = merger.resolveAssetPath(href, "js");
            if (!fs::exists(scriptPath)) {
                return std::nullopt;
            }
            const auto t = fs::last_write_time(scriptPath);
            if (t > newestInput) {
                newestInput = t;
            }
        }
        if (newestInput > bundleTime) {
            return std::nullopt;
        }
    } catch (const std::exception&) {
        return std::nullopt;
    }

    TemplateMergedJsInfo info;
    info.diskReuse = true;
    info.htmlTemplatePath = templatePath;
    info.mergedJsHrefs = std::move(disc.jsHrefs);
    return info;
}

bool mergedBundleObfuscationCacheValid(const ServerData& serverData,
                                       const std::string& cacheFilePath,
                                       const std::string& htmlTemplatePath,
                                       const std::vector<std::string>& jsHrefs) {
    namespace fs = std::filesystem;
    if (!fs::exists(cacheFilePath)) {
        return false;
    }

    try {
        const auto cacheTime = fs::last_write_time(cacheFilePath);
        auto newestInput = fs::last_write_time(htmlTemplatePath);

        AssetMerger merger(serverData.getRoot(), serverData.getRemoveComments(),
                           serverData.getObfuscationExclusions());
        for (const auto& href : jsHrefs) {
            const std::string scriptPath = merger.resolveAssetPath(href, "js");
            if (!fs::exists(scriptPath)) {
                // Source removed since cache was built — merged bundle would differ; do not reuse cache.
                return false;
            }
            const auto t = fs::last_write_time(scriptPath);
            if (t > newestInput) {
                newestInput = t;
            }
        }

        if (newestInput > cacheTime) {
            return false;
        }

        const auto now = fs::file_time_type::clock::now();
        const auto age =
            std::chrono::duration_cast<std::chrono::hours>(now - cacheTime).count() / 24;
        return age < serverData.getObfuscationCacheExpiry();
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace

JSBuilder::JSBuilder(const std::string &inputPath, const ServerData& serverData) 
    : ContentBuilder(inputPath, serverData) {
    builJS();
}

void JSBuilder::builJS() {
    // Extract filename from path for exclusion checking
    std::filesystem::path filePath(path);
    std::string filename = filePath.filename().string();
    
    // Check if file is excluded from obfuscation and merging
    if (isExcludedFromObfuscation(filename)) {
        // File is excluded - serve as-is (no obfuscation, no merging)
        // Just handle comment removal if enabled
        if (_serverData.getRemoveComments() && !builtFile.empty()) {
            builtFile = removeCommentsFromString(builtFile, FILETYPE_JS);
        }
        return;
    }
    
    // In dev mode with merging, check if content is in cache first
    if (tryLoadMergedAssetDevCache(path, _serverData, builtFile)) {
        return;
    }

    std::optional<TemplateMergedJsInfo> templateMerge = tryReuseOnDiskMergedBundle(path, _serverData, builtFile);
    if (!templateMerge.has_value()) {
        templateMerge = tryTemplateMergedJs(path, _serverData);
        if (templateMerge.has_value() && !templateMerge->diskReuse) {
            builtFile = std::move(templateMerge->mergedJs);
        }
    }

    // Comment removal: mergeJsFiles already strips per segment when removeComments is on;
    // avoid a second full-bundle removeJsComments pass for template-merged or on-disk merged output.
    if (_serverData.getRemoveComments() && !builtFile.empty() && !templateMerge.has_value()) {
        builtFile = removeCommentsFromString(builtFile, FILETYPE_JS);
    }
    
    // Check if obfuscation should be applied
    // (only if not dev mode and obfuscation level > 0)
    if (!_serverData.shouldObfuscate()) {
        // No obfuscation needed
        return;
    }
    
    // Check if we have a valid cached obfuscated version
    std::string cacheFilePath = path;
    size_t jsPos = cacheFilePath.rfind(".js");
    if (jsPos != std::string::npos) {
        cacheFilePath.replace(jsPos, 3, ".obfuscated.js");
    } else {
        cacheFilePath += ".obfuscated";
    }

    const bool usedTemplateMerge = templateMerge.has_value();
    
    if (!usedTemplateMerge && hasValidObfuscationCache(path)) {
        // Load from cache
        try {
            builtFile = ContentBuilder::loadFile(cacheFilePath);
        } catch (const std::exception&) {
            // Failed to load cache - fall through to re-obfuscate
        }
    } else if (usedTemplateMerge &&
               mergedBundleObfuscationCacheValid(_serverData, cacheFilePath,
                                                 templateMerge->htmlTemplatePath,
                                                 templateMerge->mergedJsHrefs)) {
        try {
            builtFile = ContentBuilder::loadFile(cacheFilePath);
        } catch (const std::exception&) {
            try {
                builtFile = obfuscateAndCache(builtFile, cacheFilePath);
            } catch (const std::exception&) {
                if (_serverData.getObfuscationStrictUndefined()) {
                    throw;
                }
            }
        }
    } else {
        // Need to obfuscate and cache
        try {
            builtFile = obfuscateAndCache(builtFile, cacheFilePath);
        } catch (const std::exception&) {
            if (_serverData.getObfuscationStrictUndefined()) {
                throw;
            }
            // Obfuscation failed - builtFile still contains original content
        }
    }
}

bool JSBuilder::isExcludedFromObfuscation(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string filename = p.filename().string();
    return _serverData.isObfuscationExcluded(filename);
}

bool JSBuilder::hasValidObfuscationCache(const std::string& filePath) {
    namespace fs = std::filesystem;
    
    // Generate cache file path: replace .js with .obfuscated.js
    std::string cacheFilePath = filePath;
    size_t jsPos = cacheFilePath.rfind(".js");
    if (jsPos != std::string::npos) {
        cacheFilePath.replace(jsPos, 3, ".obfuscated.js");
    } else {
        cacheFilePath += ".obfuscated";
    }
    
    // Check if both source and cache files exist
    if (!fs::exists(filePath) || !fs::exists(cacheFilePath)) {
        return false;
    }
    
    try {
        // Get modification times
        auto sourceTime = fs::last_write_time(filePath);
        auto cacheTime = fs::last_write_time(cacheFilePath);
        auto now = fs::file_time_type::clock::now();
        
        // Cache is invalid if source is newer than cache
        if (sourceTime > cacheTime) {
            return false;
        }
        
        // Calculate cache age in days
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - cacheTime).count() / 24;
        
        // Check if within expiry time
        int expiryDays = _serverData.getObfuscationCacheExpiry();
        return age < expiryDays;
        
    } catch (const std::exception&) {
        // Error checking cache validity - consider invalid
        return false;
    }
}

std::string JSBuilder::obfuscateAndCache(const std::string& content, const std::string& cacheFilePath) {
    JSObfuscateSettings st;
    st.preserveIdentNames = _serverData.getObfuscationPreserveIdents();
    st.externGlobalNames = _serverData.getObfuscationExternGlobals();
    st.strictUndefinedSymbols = _serverData.getObfuscationStrictUndefined();
    st.emitGlobalThisAssignments = _serverData.getObfuscationEmitGlobalThisAssignments();
    st.validateOutputWithAcorn = _serverData.getObfuscationValidateWithAcorn();
    st.autoPreserveBracketStringKeys = _serverData.getObfuscationAutoBracketKeys();

    JSObfuscator obfuscator(_serverData.getObfuscationLevel(), st);

    std::string obfuscated = obfuscator.obfuscate(content);
    
    // Save to disk cache file (e.g., utils.obfuscated.js)
    try {
        FileManagement::saveFile(cacheFilePath, obfuscated);
    } catch (const std::exception&) {
        // Failed to cache - continue anyway
    }
    
    return obfuscated;
}

}  // namespace geruest
