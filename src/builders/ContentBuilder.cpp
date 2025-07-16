/**
 * @file ContentBuilder.cpp
 * @date 16.07.25
 *
 * @author Urs Behrmann
 *
 * @brief This class is the base for all content builders
 */

#include "ContentBuilder.hpp"

ContentBuilder::ContentBuilder(std::string pathReceived, std::string serverRoot) 
    : root(std::move(serverRoot)), path(std::move(pathReceived)) {
    builtFile = loadFile(path);
}

std::string ContentBuilder::sizeString() const {
    return std::to_string(builtFile.size());
}

size_t ContentBuilder::size() const {
    return builtFile.size();
}

std::string ContentBuilder::file() const {
    return builtFile;
}

std::string ContentBuilder::loadFile(const std::string& pathReceived) {
    std::ifstream fileStream(pathReceived);

    if (!fileStream) return "";

    std::stringstream buffer;
    buffer << fileStream.rdbuf();

    fileStream.close();

    return buffer.str();
}
