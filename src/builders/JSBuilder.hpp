/**
 * @file JSBuilder.hpp
 * @date on: 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * 
 * When mergeAssets=false: Serves individual JS files as-is.
 * When mergeAssets=true: Serves pre-generated merged JS files created by HTMLBuilder.
 */

#ifndef JSBUILDER_HPP
#define JSBUILDER_HPP

#include "ContentBuilder.hpp"
#include <string>
#include <vector>

namespace geruest {

class JSBuilder : public ContentBuilder {
public:

    JSBuilder(const std::string &inputPath, const ServerData& serverData);

private:

    void builJS();
};

}  // namespace geruest

#endif //JSBUILDER_HPP