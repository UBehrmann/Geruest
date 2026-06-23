/**
 * @file ModuleHooks.hpp
 * @brief Optional module registration points (Assets, WebSocket, Email).
 */

#ifndef GERUEST_MODULEHOOKS_HPP
#define GERUEST_MODULEHOOKS_HPP

#include <boost/asio/awaitable.hpp>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace geruest {

class Handler;
class HTTPRequest;
class ServerData;

class Geruest;

namespace modules {

using TextContentProcessor = std::function<std::optional<std::string>(
    std::string_view contentType, const std::string& absPath, const ServerData& serverData)>;

using MergedAssetOwnerFn = std::function<std::optional<std::string>(
    const ServerData& serverData, const std::string& assetRequestPath)>;

using WebSocketUpgradeFn =
    std::function<boost::asio::awaitable<bool>(Handler& handler, HTTPRequest* request)>;

using EmailConfigApplierFn = std::function<void(Geruest&)>;

using ServerDataBinderFn = std::function<void(ServerData* serverData)>;

using WebpMaxDimensionFn = std::function<void(int maxDimension)>;

using WebpConvertImageFn = std::function<bool(const std::string& sourcePath, const std::string& outputPath,
                                              bool cacheOnly, float quality)>;

void registerTextContentProcessor(TextContentProcessor fn);
void registerMergedAssetOwnerFn(MergedAssetOwnerFn fn);
void registerWebSocketUpgrade(WebSocketUpgradeFn fn);
void registerEmailConfigApplier(EmailConfigApplierFn fn);
void registerServerDataBinder(ServerDataBinderFn fn);
void registerWebpMaxDimensionSetter(WebpMaxDimensionFn fn);
void registerWebpConvertImage(WebpConvertImageFn fn);

const TextContentProcessor& textContentProcessor();
const MergedAssetOwnerFn& mergedAssetOwnerFn();
const WebSocketUpgradeFn& webSocketUpgradeFn();
const EmailConfigApplierFn& emailConfigApplier();

void bindServerData(ServerData* serverData);

void setWebpMaxDimension(int maxDimension);

bool convertWebpImage(const std::string& sourcePath, const std::string& outputPath, bool cacheOnly, float quality);

std::optional<std::string> processTextContent(std::string_view contentType, const std::string& absPath,
                                             const ServerData& serverData);

std::optional<std::string> findMergedAssetOwnerPage(const ServerData& serverData,
                                                    const std::string& assetRequestPath);

/** Read a file as raw bytes/string (Core passthrough when no Assets processor). */
std::optional<std::string> readTextFileRaw(const std::string& absPath);

}  // namespace modules
}  // namespace geruest

#endif  // GERUEST_MODULEHOOKS_HPP
