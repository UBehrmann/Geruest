/**
 * @file CSSBuilder.hpp
 * @date 15.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the CSS files.
 */

#ifndef CSSBUILDER_HPP
#define CSSBUILDER_HPP

#include "ContentBuilder.hpp"
#include <string>
#include <vector>
#include "parser/JSONParser.hpp"

namespace geruest {

class CSSBuilder : public ContentBuilder {
 public:

  CSSBuilder(const std::string& path, const std::string& serverRoot, bool removeComments = true);

 private:

  JSONParser* json;
  std::string pageName;

  static std::string getFileNameWithoutExtension(const std::string& path);

  void builCSS();

};

}  // namespace geruest

#endif //CSSBUILDER_HPP