/**
 * @file Geruest.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief
 */

#include "Geruest.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <iostream>
#include <thread>

#include "data/HTTPResponse.hpp"

Geruest::Geruest() {

}

Geruest::~Geruest() {
    close(this->server_fd);
    sendToLogger("Server closed.");
}

void Geruest::setPort(int _port) { port = _port; }
void Geruest::setHostname(const std::string &hostname) { hostname_ = hostname; }

void Geruest::addRoute(const std::string &path, RouteHandler routeHandler) {
    serverData.routes[path] = std::move(routeHandler);
}

void Geruest::addRoot(const std::string &root) { serverData.root = root; }

void Geruest::init() {

    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->server_fd == 0) {
        sendToLoggerError("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    this->address.sin_family = AF_INET;
    this->address.sin_addr.s_addr = INADDR_ANY;
    this->address.sin_port = htons(port);

    if (bind(this->server_fd, (struct sockaddr *)&this->address, sizeof(this->address)) < 0) {
        sendToLoggerError("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(this->server_fd, 3) < 0) {
        sendToLoggerError("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Setting timeout for accepting connections (e.g., 5 seconds)
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;    // Timeout in seconds
    timeout.tv_usec = TIMEOUT_USEC;  // No additional microseconds

    // Set socket option for receive timeout

    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout) < 0) {
        sendToLoggerError("Failed to set send buffer size");
        exit(EXIT_FAILURE);
    }

    int send_buffer_size = BUFFER_SIZE;
    int receive_buffer_size = BUFFER_SIZE;

    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) < 0) {
        sendToLoggerError("Failed to set send buffer size");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, &receive_buffer_size, sizeof(receive_buffer_size)) < 0) {
        sendToLoggerError("Failed to set receive buffer size");
        exit(EXIT_FAILURE);
    }

    sendToLogger("Server started on port " + std::to_string(port));
}

void Geruest::start() {
    int addrlen = sizeof(this->address);

    sendToLogger("Waiting for connections...");

    while (running) {
        try {
            int new_socket = accept(this->server_fd, (struct sockaddr *)&this->address, (socklen_t *)&addrlen);

            if (new_socket < 0)
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    //                sendToLogger("Timeout for accepting connections.");
                    continue;
                } else {
                    sendToLoggerError("Accept failed");
                    continue;
                }

            // Get client's IP address
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::string client_ip_str(client_ip);

            giveToHandler(new_socket, client_ip_str);

        } catch (const std::exception &e) {
            sendToLoggerError(std::string("Exception in server loop: ") + e.what());
            continue;
        }
    }

    sendToLogger("Server stopped.");
}

void Geruest::giveToHandler(int new_socket, std::string &IP) {
    if (auto clientHandler = std::make_unique<Handler>(new_socket, IP, &serverData)) {
        // sendToLogger("New connection");

        std::thread clientThread([handler = std::move(clientHandler)]() mutable {
            try {
                handler->run();
            } catch (const std::exception &e) {
                handler->sendToLoggerError(std::string("Handler error: ") + e.what());
            } catch (...) {
                handler->sendToLoggerError("Handler encountered an unknown error");
            }
        });

        clientThread.detach();

    } else {
        sendToLoggerError("Failed to create new handler");
    }
}

void Geruest::stop() {
    sendToLogger("Stopping server at next opportunity.");

    running = false;
}

bool Geruest::isRunning() { return running; }

void Geruest::sendToLogger(const std::string &message) const {
	std::cout << message << std::endl;
}

void Geruest::sendToLoggerError(const std::string &message) const {
	std::cerr << "Error: " << message << std::endl;
}
