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

    ContentBuilder(std::string pathReceived, std::string serverRoot) : root(std::move(serverRoot)), path(std::move(pathReceived)) {
        builtFile = loadFile(path);
    }

    [[nodiscard]] std::string sizeString() const {
        return std::to_string(builtFile.size());
    }

    [[nodiscard]] size_t size() const {
        return builtFile.size();
    }

    [[nodiscard]] std::string file() const {
        return builtFile;
    }

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
    [[nodiscard]] static std::string loadFile(const std::string& pathReceived) {

        std::ifstream fileStream(pathReceived);

        if (!fileStream) return "";

        std::stringstream buffer;
        buffer << fileStream.rdbuf();

        fileStream.close();

        return buffer.str();
    }
};

#endif //CONTENTBUILDER_HPP