/**
 * @file ServerData.hpp
 * @date 11.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief This file contains the ServerData struct, which holds the server's data
 */

#ifndef GERUEST_SERVERDATA_HPP
#define GERUEST_SERVERDATA_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

namespace geruest {

using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;


// class with the server data
class ServerData {
private:
    std::unordered_map<std::string, RouteHandler> _routes;
    std::string _root;
    bool _removeComments = true;

public:
    ServerData() = default;

    ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string root)
        : _routes(routes), _root(std::move(root)) {}

    std::unordered_map<std::string, RouteHandler>& getRoutes() { return _routes; }
    const std::unordered_map<std::string, RouteHandler>& getRoutes() const { return _routes; }
    
    void addRoute(const std::string& path, RouteHandler routeHandler) {
        _routes[path] = std::move(routeHandler);
    }

    std::string getRoot() const { return _root; }
    void setRoot(const std::string& newRoot) { _root = newRoot; }

    bool getRemoveComments() const { return _removeComments; }
    void setRemoveComments(bool value) { _removeComments = value; }

    void keepComments() { _removeComments = false; }
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP