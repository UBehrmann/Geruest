#include "modules/ModuleHooks.hpp"

#include <fstream>

namespace geruest::modules {

namespace {

TextContentProcessor g_textContent;
MergedAssetOwnerFn   g_mergedAssetOwner;
WebSocketUpgradeFn   g_webSocketUpgrade;
EmailConfigApplierFn g_emailConfigApplier;
ServerDataBinderFn   g_serverDataBinder;
WebpMaxDimensionFn   g_webpMaxDimension;
WebpConvertImageFn   g_webpConvertImage;

}  // namespace

void registerWebpMaxDimensionSetter(WebpMaxDimensionFn fn) { g_webpMaxDimension = std::move(fn); }

void registerWebpConvertImage(WebpConvertImageFn fn) { g_webpConvertImage = std::move(fn); }

void registerServerDataBinder(ServerDataBinderFn fn) { g_serverDataBinder = std::move(fn); }

void registerTextContentProcessor(TextContentProcessor fn) { g_textContent = std::move(fn); }

void registerMergedAssetOwnerFn(MergedAssetOwnerFn fn) { g_mergedAssetOwner = std::move(fn); }

void registerWebSocketUpgrade(WebSocketUpgradeFn fn) { g_webSocketUpgrade = std::move(fn); }

void registerEmailConfigApplier(EmailConfigApplierFn fn) { g_emailConfigApplier = std::move(fn); }

const TextContentProcessor& textContentProcessor() { return g_textContent; }

const MergedAssetOwnerFn& mergedAssetOwnerFn() { return g_mergedAssetOwner; }

const WebSocketUpgradeFn& webSocketUpgradeFn() { return g_webSocketUpgrade; }

const EmailConfigApplierFn& emailConfigApplier() { return g_emailConfigApplier; }

void bindServerData(ServerData* serverData) {
    if (g_serverDataBinder && serverData != nullptr) {
        g_serverDataBinder(serverData);
    }
}

void setWebpMaxDimension(int maxDimension) {
    if (g_webpMaxDimension) {
        g_webpMaxDimension(maxDimension);
    }
}

bool convertWebpImage(const std::string& sourcePath, const std::string& outputPath, bool cacheOnly,
                      float quality) {
    if (g_webpConvertImage) {
        return g_webpConvertImage(sourcePath, outputPath, cacheOnly, quality);
    }
    return false;
}

std::optional<std::string> processTextContent(std::string_view contentType, const std::string& absPath,
                                              const ServerData& serverData) {
    if (g_textContent) {
        return g_textContent(contentType, absPath, serverData);
    }
    return readTextFileRaw(absPath);
}

std::optional<std::string> findMergedAssetOwnerPage(const ServerData& serverData,
                                                    const std::string& assetRequestPath) {
    if (g_mergedAssetOwner) {
        return g_mergedAssetOwner(serverData, assetRequestPath);
    }
    return std::nullopt;
}

std::optional<std::string> readTextFileRaw(const std::string& absPath) {
    std::ifstream in(absPath, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace geruest::modules
