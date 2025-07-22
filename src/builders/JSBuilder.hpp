/**
 * @file JSBuilder.hpp
 * @date on: 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to build the JavaScript files.
 * It looks for the page name in the map and includes the files that are associated with it.
 * If the page name is not found, it returns the JS files associated with the path.
 */

#ifndef JSBUILDER_HPP
#define JSBUILDER_HPP

#include "ContentBuilder.hpp"
#include <string>
#include <vector>
#include "parser/JSONParser.hpp"

namespace geruest {

class JSBuilder : public ContentBuilder {
public:

    JSBuilder(const std::string& path, const std::string& serverRoot);

private:

    JSONParser* json;

    std::string pageName;

    static std::string getFileNameWithoutExtension(const std::string& path);

    void builJS();
};

}  // namespace geruest

#endif //JSBUILDER_HPP