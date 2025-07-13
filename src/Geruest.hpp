/**
 * @file Geruest.hpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief
 */

#ifndef GERUEST_GERUEST_HPP
#define GERUEST_GERUEST_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  // For close

#include <cstring>  // For memset
#include <functional>
#include <string>
#include <unordered_map>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "handler/Handler.hpp"
#include "data/ServerData.hpp"

// Constants
#define TIMEOUT_SEC 30
#define TIMEOUT_USEC 0

// Max packet size
#define BUFFER_SIZE 8192

class Geruest {
   public:
    Geruest();
    ~Geruest();

    void setPort(int port);

    void setHostname(const std::string& hostname);

    void addRoute(const std::string& path, RouteHandler handler);

    void addRoot(const std::string& root);

    /*
     * Initializes the server, sets up the socket, binds it to the address and port,
     * and prepares it to listen for incoming connections.
     * This method should be called before starting the server.
     */
    void init();

    void start();

    void stop();

   private:
    int server_fd = -1;  // Socket descriptor for the server

    struct sockaddr_in address = {0};

    bool running = true;

    Logger& logger = Logger::getInstance();

    int port = 8080;

    std::string hostname_ = "localhost";

    ServerData serverData;

    bool isRunning();

    void sendToLogger(const std::string& message) const;

    void sendToLoggerError(const std::string& message) const;

    void giveToHandler(int new_socket, std::string& IP);
};

#endif  // GERUEST_GERUEST_HPP
