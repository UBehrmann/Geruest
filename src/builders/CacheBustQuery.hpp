/**
 * @file CacheBustQuery.hpp
 * @brief Append ?v=TOKEN to static asset URLs in HTML.
 */

#ifndef GERUEST_CACHEBUSTQUERY_HPP
#define GERUEST_CACHEBUSTQUERY_HPP

#include <string>
#include <string_view>

namespace geruest {

/** Mutates @p html in place. Skips external URLs and non-static assets. */
void appendCacheBustToHtml(std::string& html, std::string_view token);

/** Append cache-bust query param to a single URL (href or src value). */
std::string appendCacheBustParam(std::string_view url, std::string_view token);

}  // namespace geruest

#endif  // GERUEST_CACHEBUSTQUERY_HPP
