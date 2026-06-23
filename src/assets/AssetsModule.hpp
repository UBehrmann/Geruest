#ifndef GERUEST_ASSETS_ASSETSMODULE_HPP
#define GERUEST_ASSETS_ASSETSMODULE_HPP

namespace geruest {

class ServerData;

namespace assets {

/** Registers text-content and merged-asset hooks (call once when the Assets module is linked). */
void ensureAssetsModuleRegistered();

/** Linker anchor — referenced from Geruest constructor when Assets is enabled. */
int& assetsModuleLinkAnchor();

}  // namespace assets
}  // namespace geruest

#endif
