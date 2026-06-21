/**
 * @file ObfuscationSettings.hpp
 * @brief JavaScript obfuscation configuration.
 */

#ifndef GERUEST_OBFUSCATIONSETTINGS_HPP
#define GERUEST_OBFUSCATIONSETTINGS_HPP

#include <string>
#include <unordered_set>
#include <vector>

namespace geruest {

class ObfuscationSettings {
   public:
    ObfuscationSettings() = default;
    ObfuscationSettings(const ObfuscationSettings&) = default;
    ObfuscationSettings& operator=(const ObfuscationSettings&) = default;

    void setLevel(unsigned int level) { _level = level; }
    unsigned int getLevel() const { return _level; }

    void setCacheExpiryDays(int days) { _cacheExpiryDays = days; }
    int getCacheExpiryDays() const { return _cacheExpiryDays; }

    void addExclusion(const std::string& filename);
    bool isExcluded(const std::string& filename) const;
    const std::vector<std::string>& getExclusions() const { return _exclusions; }

    void addPreserveIdent(const std::string& name);
    void addExternGlobal(const std::string& name);
    const std::unordered_set<std::string>& getPreserveIdents() const { return _preserveIdents; }
    const std::unordered_set<std::string>& getExternGlobals() const { return _externGlobals; }

    void loadExternsFromText(const std::string& text);

    void setStrictUndefined(bool v) { _strictUndefined = v; }
    bool getStrictUndefined() const { return _strictUndefined; }

    void setEmitGlobalThisBracket(bool v) { _emitGlobalThisBracket = v; }
    bool getEmitGlobalThisBracket() const { return _emitGlobalThisBracket; }

    void setValidateWithAcorn(bool v) { _validateWithAcorn = v; }
    bool getValidateWithAcorn() const { return _validateWithAcorn; }

    void setAutoBracketKeys(bool v) { _autoBracketKeys = v; }
    bool getAutoBracketKeys() const { return _autoBracketKeys; }

   private:
    unsigned int _level = 0;
    int _cacheExpiryDays = 7;
    std::vector<std::string> _exclusions;
    std::unordered_set<std::string> _preserveIdents;
    std::unordered_set<std::string> _externGlobals;
    bool _strictUndefined = false;
    bool _emitGlobalThisBracket = false;
    bool _validateWithAcorn = false;
    bool _autoBracketKeys = true;
};

}  // namespace geruest

#endif  // GERUEST_OBFUSCATIONSETTINGS_HPP
