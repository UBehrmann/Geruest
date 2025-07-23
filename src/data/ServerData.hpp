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
    std::unordered_map<std::string, RouteHandler> routes;
    std::string root;
    bool removeComments = true;

public:
    ServerData() = default;

    ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string root)
        : routes(routes), root(std::move(root)) {}

    std::unordered_map<std::string, RouteHandler>& getRoutes() { return routes; }
    const std::unordered_map<std::string, RouteHandler>& getRoutes() const { return routes; }
    
    void addRoute(const std::string& path, RouteHandler routeHandler) {
        routes[path] = std::move(routeHandler);
    }

    std::string getRoot() const { return root; }
    void setRoot(const std::string& newRoot) { root = newRoot; }

    bool getRemoveComments() const { return removeComments; }
    void setRemoveComments(bool value) { removeComments = value; }
    
    void keepComments() { removeComments = false; }
};

}  // namespace geruest

#endif  // GERUEST_SERVERDATA_HPP