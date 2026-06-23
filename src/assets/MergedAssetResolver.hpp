#ifndef GERUEST_ASSETS_MERGEDASSETRESOLVER_HPP
#define GERUEST_ASSETS_MERGEDASSETRESOLVER_HPP

#include <optional>
#include <string>

namespace geruest {

class ServerData;

namespace assets {

std::optional<std::string> findMergedAssetOwnerPagePath(const ServerData& serverData,
                                                        const std::string& assetRequestPath);

}  // namespace assets
}  // namespace geruest

#endif
