#include "assets/MergedAssetResolver.hpp"

#include <filesystem>
#include <fstream>

#include "assets/MergedAssetResolver.hpp"
#include "assets/AssetsModule.hpp"
#include "builders/AssetMerger.hpp"
#include "data/ServerData.hpp"
#include "data/ServerTypes.hpp"

namespace geruest::assets {

std::optional<std::string> findMergedAssetOwnerPagePath(const ServerData& serverData,
                                                        const std::string& assetRequestPath) {
    ensureAssetsModuleRegistered();
    if (!serverData.getMergeAssets() || serverData.getRoot().empty()) {
        return std::nullopt;
    }

    const std::string canon = canonicalRequestPath(assetRequestPath);
    const bool isJs = canon.size() > 3 && canon.compare(canon.size() - 3, 3, ".js") == 0;
    const bool isCss = canon.size() > 4 && canon.compare(canon.size() - 4, 4, ".css") == 0;
    if (!isJs && !isCss) {
        return std::nullopt;
    }

    const size_t dotPos = canon.find_last_of('.');
    if (dotPos == std::string::npos || dotPos <= 1) {
        return std::nullopt;
    }

    const size_t lastSlash = canon.find_last_of('/');
    const std::string pageStem = (lastSlash == std::string::npos) ? canon.substr(1, dotPos - 1)
                                                                  : canon.substr(lastSlash + 1, dotPos - lastSlash - 1);
    if (pageStem.empty()) {
        return std::nullopt;
    }

    const std::string htmlRoot = serverData.getRoot() + "/html";
    if (!std::filesystem::is_directory(htmlRoot)) {
        return std::nullopt;
    }

    const std::string templatePath = AssetMerger::findHtmlTemplateByPageName(htmlRoot, pageStem);
    if (templatePath.empty()) {
        return std::nullopt;
    }

    std::ifstream in(templatePath, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::string htmlContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (htmlContent.empty()) {
        return std::nullopt;
    }

    AssetMerger merger(serverData.getRoot(), serverData.getRemoveComments(), serverData.getObfuscationExclusions());
    const std::string pageName = AssetMerger::pageNameFromHtmlPath(templatePath);
    const std::vector<std::string> predicted = merger.predictMergedAssetUrls(htmlContent, pageName);
    for (const std::string& url : predicted) {
        if (canonicalRequestPath(url) == canon) {
            return AssetMerger::sitePathFromHtmlFile(htmlRoot, templatePath);
        }
    }
    return std::nullopt;
}

}  // namespace geruest::assets
