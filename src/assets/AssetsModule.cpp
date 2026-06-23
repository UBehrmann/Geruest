#include "assets/AssetsModule.hpp"

#include <memory>
#include <string>
#include <string_view>

#include "assets/MergedAssetResolver.hpp"
#include "builders/ContentBuilder.hpp"
#include "builders/WebPConverter.hpp"
#include "data/ServerData.hpp"
#include "modules/ModuleHooks.hpp"

namespace geruest::assets {

namespace {

std::optional<std::string> processTextContentImpl(std::string_view contentType, const std::string& absPath,
                                                  const ServerData& serverData) {
    std::unique_ptr<ContentBuilder> builder = ContentBuilder::create(std::string(contentType), absPath, serverData);
    if (!builder || builder->size() == 0) {
        return std::nullopt;
    }
    return builder->file();
}

struct AssetsModuleRegistrar {
    AssetsModuleRegistrar() {
        modules::registerTextContentProcessor(processTextContentImpl);
        modules::registerMergedAssetOwnerFn(
            [](const ServerData& sd, const std::string& path) { return findMergedAssetOwnerPagePath(sd, path); });
        modules::registerServerDataBinder([](ServerData* sd) { WebPConverter::setServerData(sd); });
        modules::registerWebpMaxDimensionSetter([](int dim) { WebPConverter::setMaxConversionDimension(dim); });
        modules::registerWebpConvertImage(
            [](const std::string& src, const std::string& out, bool cacheOnly, float quality) {
                return WebPConverter::convertImage(src, out, cacheOnly, quality);
            });
    }
};

}  // namespace

void ensureAssetsModuleRegistered() {
    static const AssetsModuleRegistrar instance;
    (void)instance;
}

int& assetsModuleLinkAnchor() {
    static int anchor = 0;
    return anchor;
}

}  // namespace geruest::assets
