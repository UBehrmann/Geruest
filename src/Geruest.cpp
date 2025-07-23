/**
 * @file Geruest.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief
 */

#include "Geruest.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <iostream>
#include <thread>

#include "data/HTTPResponse.hpp"

namespace geruest {

Geruest::Geruest() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        sendToLoggerError("WSAStartup failed: " + std::to_string(result));
        exit(EXIT_FAILURE);
    }
#endif
}

Geruest::~Geruest() {
#ifdef _WIN32
    if (server_fd != INVALID_SOCKET) {
        closesocket(server_fd);
    }
    WSACleanup();
#else
    close(this->server_fd);
#endif
    sendToLogger("Server closed.");
}

void Geruest::setPort(int _port) { port = _port; }
void Geruest::setHostname(const std::string &hostname) { hostname_ = hostname; }

void Geruest::addRoute(const std::string &path, RouteHandler routeHandler) {
    serverData.addRoute(path, std::move(routeHandler));
}

void Geruest::addRoot(const std::string &root) { serverData.setRoot(root); }

void Geruest::init() {

    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (this->server_fd == INVALID_SOCKET) {
        sendToLoggerError("Socket creation failed: " + std::to_string(WSAGetLastError()));
#else
    if (this->server_fd == 0) {
        sendToLoggerError("Socket creation failed");
#endif
        exit(EXIT_FAILURE);
    }

    this->address.sin_family = AF_INET;
    this->address.sin_addr.s_addr = INADDR_ANY;
    this->address.sin_port = htons((unsigned short)port);

    if (bind(this->server_fd, (struct sockaddr *)&this->address, sizeof(this->address)) < 0) {
#ifdef _WIN32
        sendToLoggerError("Bind failed: " + std::to_string(WSAGetLastError()));
#else
        sendToLoggerError("Bind failed");
#endif
        exit(EXIT_FAILURE);
    }

    if (listen(this->server_fd, 3) < 0) {
#ifdef _WIN32
        sendToLoggerError("Listen failed: " + std::to_string(WSAGetLastError()));
#else
        sendToLoggerError("Listen failed");
#endif
        exit(EXIT_FAILURE);
    }

    // Setting timeout for accepting connections (e.g., 5 seconds)
#ifdef _WIN32
    DWORD timeout = TIMEOUT_SEC * 1000;  // Convert to milliseconds
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        sendToLoggerError("Failed to set receive timeout: " + std::to_string(WSAGetLastError()));
#else
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;    // Timeout in seconds
    timeout.tv_usec = TIMEOUT_USEC;  // No additional microseconds

    // Set socket option for receive timeout
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout) < 0) {
        sendToLoggerError("Failed to set send buffer size");
#endif
        exit(EXIT_FAILURE);
    }

    int send_buffer_size = BUFFER_SIZE;
    int receive_buffer_size = BUFFER_SIZE;

    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, (const char*)&send_buffer_size, sizeof(send_buffer_size)) < 0) {
#ifdef _WIN32
        sendToLoggerError("Failed to set send buffer size: " + std::to_string(WSAGetLastError()));
#else
        sendToLoggerError("Failed to set send buffer size");
#endif
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, (const char*)&receive_buffer_size, sizeof(receive_buffer_size)) < 0) {
#ifdef _WIN32
        sendToLoggerError("Failed to set receive buffer size: " + std::to_string(WSAGetLastError()));
#else
        sendToLoggerError("Failed to set receive buffer size");
#endif
        exit(EXIT_FAILURE);
    }

    sendToLogger("Server started on port " + std::to_string(port));
}

void Geruest::start() {
    int addrlen = sizeof(this->address);

    sendToLogger("Waiting for connections...");

    while (running) {
        try {
#ifdef _WIN32
            SOCKET new_socket = accept(this->server_fd, (struct sockaddr *)&this->address, &addrlen);

            if (new_socket == INVALID_SOCKET) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
#else
            int new_socket = accept(this->server_fd, (struct sockaddr *)&this->address, (socklen_t *)&addrlen);

            if (new_socket < 0){
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
                    //                sendToLogger("Timeout for accepting connections.");
                    continue;
                } else {
#ifdef _WIN32
                    sendToLoggerError("Accept failed: " + std::to_string(error));
#else
                    sendToLoggerError("Accept failed");
#endif
                    continue;
                }
            }
            // Get client's IP address
            char client_ip[INET_ADDRSTRLEN];
#ifdef _WIN32
            // Use inet_ntoa for Windows compatibility with older MinGW
            strcpy(client_ip, inet_ntoa(address.sin_addr));
#else
            inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
#endif
            std::string client_ip_str(client_ip);

            giveToHandler(new_socket, client_ip_str);

        } catch (const std::exception &e) {
            sendToLoggerError(std::string("Exception in server loop: ") + e.what());
            continue;
        }
    }

    sendToLogger("Server stopped.");
}

#ifdef _WIN32
void Geruest::giveToHandler(SOCKET new_socket, std::string &IP) {
#else
void Geruest::giveToHandler(int new_socket, std::string &IP) {
#endif
    if (auto clientHandler = std::make_unique<Handler>(new_socket, IP, serverData)) {
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

}  // namespace geruest
