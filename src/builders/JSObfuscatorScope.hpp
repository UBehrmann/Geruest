/**
 * @file JSObfuscatorScope.hpp
 * @brief Scope-aware identifier renaming for JSObfuscator (one binding → one mangled name).
 */

#ifndef GERUEST_JSOBFUSCATOR_SCOPE_HPP
#define GERUEST_JSOBFUSCATOR_SCOPE_HPP

#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace geruest::js_scope {

struct RenameSpan {
    size_t start = 0;
    size_t end = 0;
    std::string mangled;
};

struct ScopeRenamePlan {
    /// Replacement spans sorted by start (non-overlapping).
    std::vector<RenameSpan> spans;
    /// Free identifiers (no local binding) that are not reserved/externs.
    std::vector<std::string> undefinedSymbols;
    /// Human-readable messages (warnings / soft diagnostics).
    std::vector<std::string> warnings;
    bool usedLegacyFallback = false;
};

struct ScopeRenameOptions {
    const std::unordered_set<std::string>* reserved = nullptr;
    std::unordered_set<std::string> preserve;
    std::unordered_set<std::string> externNames;
    std::function<std::string()> generateMangledName;
    /// When true, free identifiers (no lexical binding) are reported in undefinedSymbols and left
    /// unchanged so the caller can fail. When false, legacy spelling-keyed mangling applies (same as
    /// old obfuscator for globals / skipped regions like class bodies).
    bool strictFreeIdentifiers = false;
};

/// Build per-occurrence renames using lexical scopes (var hoisting, let/const blocks, functions, catch).
ScopeRenamePlan computeScopedRenames(const std::string& code, const ScopeRenameOptions& opt,
                                     std::vector<std::string>* topLevelPreservedNamesOut = nullptr);

}  // namespace geruest::js_scope

#endif
