/**
 * @file ContentBuilder.hpp
 * @date 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This class is the base for all content builders
 */

#ifndef CONTENTBUILDER_HPP
#define CONTENTBUILDER_HPP

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

class ContentBuilder {
public:

    ContentBuilder(std::string pathReceived, std::string serverRoot);

    [[nodiscard]] std::string sizeString() const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] std::string file() const;

protected:

    const std::string root;
    std::string builtFile;
	std::string path;

	/**
	 * Load a file
	 *
	 * @param pathReceived
	 * @return The content of the file
	 */
    [[nodiscard]] static std::string loadFile(const std::string& pathReceived);
};

#endif //CONTENTBUILDER_HPP