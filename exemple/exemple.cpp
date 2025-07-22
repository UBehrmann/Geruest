/**
 * @file exemple.cpp
 * @date 11.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Example of how to use the Geruest server
 */

 #include <iostream>
#include <string>
#include <csignal>
#include <filesystem>
#include "Geruest.hpp"

#define PORT 80
#define HOSTNAME "localhost"

using namespace geruest;

Geruest* server;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    server->stop();
}

void addRoutes(Geruest* serverToAddRoutes);

int main(int argc, char* argv[]) {

    server = new Geruest();

    // Set the signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server->setPort(PORT);
    server->setHostname(HOSTNAME);
    
    // Get the absolute path to the website folder next to the executable
    std::filesystem::path executablePath = std::filesystem::canonical(std::filesystem::path(argv[0]).parent_path());
    std::filesystem::path websitePath = executablePath / "website";
    
    server->addRoot(websitePath.string());

    addRoutes(server);

    std::cout << "Starting Geruest server on port " << PORT << "..." << std::endl;
    std::cout << "Server will be accessible at http://" << HOSTNAME << ":" << PORT << std::endl;
    std::cout << "Press Ctrl+C to stop the server." << std::endl;

    server->init();

    server->start();

    // Clean up
    delete server;
    
    return EXIT_SUCCESS;
}

void addRoutes(Geruest* serverToAddRoutes) {
    // Example GET route
    serverToAddRoutes->addRoute("/test", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/html");
        response.setBody("<h1>Hello, World!</h1><p>Welcome to Geruest server!</p>");
        return response;
    });

    // GET endpoint
    serverToAddRoutes->addRoute("/api/get", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"method": "GET", "message": "GET request received!"})");
        return response;
    });

    // POST endpoint
    serverToAddRoutes->addRoute("/api/post", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({\"method\": \"POST\", \"message\": \"POST request received!\", \"body\": ")" + req.getBody() + "}");
        return response;
    });

    // PUT endpoint
    serverToAddRoutes->addRoute("/api/put", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({\"method\": \"PUT\", \"message\": \"PUT request received!\", \"body\": ")" + req.getBody() + "}");
        return response;
    });

    // DELETE endpoint
    serverToAddRoutes->addRoute("/api/delete", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({\"method\": \"DELETE\", \"message\": \"DELETE request received!\"})");
        return response;
    });
}