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

using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;

// struct with the server data
struct ServerData {
    std::unordered_map<std::string, RouteHandler> routes;
    std::string root;

    ServerData() = default;

    ServerData(const std::unordered_map<std::string, RouteHandler>& routes, std::string  root)
        : routes(routes), root(std::move(root)) {}

    std::unordered_map<std::string, RouteHandler>& getRoutes() { return routes; }

    std::string getRoot() const { return root; }
};

#endif  // GERUEST_SERVERDATA_HPP