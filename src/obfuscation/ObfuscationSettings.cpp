#include "obfuscation/ObfuscationSettings.hpp"

#include <cctype>

namespace geruest {

void ObfuscationSettings::addExclusion(const std::string& filename) {
    _exclusions.push_back(filename);
}

bool ObfuscationSettings::isExcluded(const std::string& filename) const {
    for (const auto& excluded : _exclusions) {
        if (filename == excluded || filename.find(excluded) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void ObfuscationSettings::addPreserveIdent(const std::string& name) {
    if (!name.empty()) {
        _preserveIdents.insert(name);
    }
}

void ObfuscationSettings::addExternGlobal(const std::string& name) {
    if (!name.empty()) {
        _externGlobals.insert(name);
    }
}

void ObfuscationSettings::loadExternsFromText(const std::string& text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string line = text.substr(pos, end - pos);
        size_t a = 0;
        while (a < line.size() && std::isspace(static_cast<unsigned char>(line[a]))) {
            ++a;
        }
        size_t b = line.size();
        while (b > a && std::isspace(static_cast<unsigned char>(line[b - 1]))) {
            --b;
        }
        line = line.substr(a, b - a);
        if (!line.empty() && line[0] != '#') {
            addExternGlobal(line);
        }
        pos = end + 1;
    }
}

}  // namespace geruest
