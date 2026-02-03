/**
 * @file CSSBuilder.hpp
 * @date 15.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the CSS files.
 * 
 * When mergeAssets=false: Serves individual CSS files as-is.
 * When mergeAssets=true: Serves pre-generated merged CSS files created by HTMLBuilder.
 */

#ifndef CSSBUILDER_HPP
#define CSSBUILDER_HPP

#include "ContentBuilder.hpp"
#include <string>
#include <vector>

namespace geruest {

class CSSBuilder : public ContentBuilder {
 public:

  CSSBuilder(const std::string &inputPath, const ServerData& serverData);

 private:

  void builCSS();

};

}  // namespace geruest

#endif //CSSBUILDER_HPP