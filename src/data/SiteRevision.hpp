/**
 * @file SiteRevision.hpp
 * @brief Content revision token for cache-busting query parameters.
 */

#ifndef GERUEST_SITEREVISION_HPP
#define GERUEST_SITEREVISION_HPP

#include <string>

namespace geruest {

/** FNV-1a 64-bit digest as 16 lowercase hex chars. */
std::string fnv1a64Hex(std::string_view data);

/**
 * Hash of (relative path + mtime) for all regular files under @p websiteRoot.
 * ponytail: O(n) scan at init(); empty root → "0".
 */
std::string computeSiteRevision(const std::string& websiteRoot);

}  // namespace geruest

#endif  // GERUEST_SITEREVISION_HPP
