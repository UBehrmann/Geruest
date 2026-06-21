/**
 * @file WildcardMatch.hpp
 * @brief Wildcard path matching helpers shared by RouteRegistry and GateRegistry.
 */

#ifndef GERUEST_WILDCARDMATCH_HPP
#define GERUEST_WILDCARDMATCH_HPP

#include <optional>
#include <string>

namespace geruest {

bool matchesWildcardPattern(const std::string& pattern, const std::string& path);
std::optional<std::string> extractWildcardCapture(const std::string& pattern, const std::string& path);
std::string applyWildcardCapture(const std::string& target, const std::string& capture);
bool isLikelyExternalTarget(const std::string& target);

}  // namespace geruest

#endif  // GERUEST_WILDCARDMATCH_HPP
