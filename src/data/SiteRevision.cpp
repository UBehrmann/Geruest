/**
 * @file SiteRevision.cpp
 */

#include "SiteRevision.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace geruest {
namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void fnv1a64Update(std::uint64_t& hash, std::string_view data) {
    for (unsigned char byte : data) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
    }
}

}  // namespace

std::string fnv1a64Hex(std::string_view data) {
    std::uint64_t hash = kFnvOffset;
    fnv1a64Update(hash, data);
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
    return buf;
}

std::string computeSiteRevision(const std::string& websiteRoot) {
    if (websiteRoot.empty()) {
        return "0";
    }

    namespace fs = std::filesystem;
    std::uint64_t hash = kFnvOffset;
    std::error_code ec;
    const fs::path root(websiteRoot);

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return "0";
    }

    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }

        std::error_code mtimeEc;
        const auto mtime = fs::last_write_time(it->path(), mtimeEc);
        if (mtimeEc) {
            continue;
        }

        const std::string rel = it->path().lexically_relative(root).generic_string();
        fnv1a64Update(hash, rel);
        fnv1a64Update(hash, std::string_view(reinterpret_cast<const char*>(&mtime),
                                             sizeof(mtime)));
    }

    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
    return buf;
}

}  // namespace geruest
