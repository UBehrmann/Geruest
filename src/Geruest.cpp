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
#include "geruest/Version.hpp"

namespace geruest {

Geruest::Geruest() {
    // Print version information
    std::cout << "Geruest Framework v" << getVersion() << std::endl;
    
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
    // Stop workers first
    stopWorkers();

#ifdef _WIN32
    if (server_fd != INVALID_SOCKET) {
        closesocket(server_fd);
    }
    WSACleanup();
#else
    if (this->server_fd >= 0) {
        close(this->server_fd);
    }
#endif
    sendToLogger("Server closed.");
}

void Geruest::setPort(int _port) { port = _port; }
void Geruest::setHostname(const std::string& hostname) { hostname_ = hostname; }

void Geruest::addRoute(const std::string& path, RouteHandler routeHandler) {
    serverData.addRoute(path, std::move(routeHandler));
}

void Geruest::addRoot(const std::string& root) { serverData.setRoot(root); }

void Geruest::setAvailableLanguages(const std::vector<std::string>& languages) {
    serverData.setAvailableLanguages(languages);
    if (!languages.empty()) {
        sendToLogger("Available languages: " + std::to_string(languages.size()) + ", default: " + languages[0]);
    } else {
        sendToLogger("No languages configured - language routing disabled");
    }
}

void Geruest::setMergeAssets(bool enabled) {
    serverData.setMergeAssets(enabled);
    sendToLogger(std::string("Asset merging ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::setWebPConversion(bool enabled) {
    serverData.setWebPConversion(enabled);
    sendToLogger(std::string("WebP conversion ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::enableWebPConversion() {
    serverData.enableWebPConversion();
    sendToLogger("WebP conversion enabled");
}

void Geruest::setWebPQuality(float quality) {
    serverData.setWebPQuality(quality);
    sendToLogger("WebP quality set to " + std::to_string(static_cast<int>(serverData.getWebPQuality())) + "%");
}

void Geruest::enableDevMode() {
    serverData.enableDevMode();
    sendToLogger("Development mode enabled: verbose logging, no file caching, comments preserved");
}

void Geruest::setWorkerThreadCount(size_t count) {
    if (running || _workersRunning) {
        sendToLoggerError("Cannot change worker thread count while server is running");
        return;
    }
    if (count == 0) {
        sendToLoggerError("Worker thread count must be at least 1, setting to 1");
        _workerThreadCount = 1;
        return;
    }
    _workerThreadCount = count;
    sendToLogger("Worker thread count set to: " + std::to_string(_workerThreadCount));
}

void Geruest::setMaxQueueSize(size_t size) {
    if (running || _workersRunning) {
        sendToLoggerError("Cannot change queue size while server is running");
        return;
    }
    if (size == 0) {
        sendToLoggerError("Queue size must be at least 1, setting to 1");
        _maxQueueSize = 1;
        return;
    }
    _maxQueueSize = size;
    sendToLogger("Maximum queue size set to: " + std::to_string(_maxQueueSize));
}

// ========== Basic Authentication Implementation ==========

void Geruest::setBasicAuthEnabled(bool enabled) {
    serverData.getBasicAuth().setEnabled(enabled);
    sendToLogger(std::string("Basic Authentication ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::addBasicAuthUser(const std::string& username, const std::string& password) {
    serverData.getBasicAuth().addUser(username, password);
    sendToLogger("Added Basic Auth user: " + username + " (password hashed with SHA-256)");
}

void Geruest::addBasicAuthUserHashed(const std::string& username, const std::string& hashedPassword) {
    serverData.getBasicAuth().addUserHashed(username, hashedPassword);
    sendToLogger("Added Basic Auth user: " + username + " (using pre-hashed password)");
}

std::string Geruest::hashPassword(const std::string& password) {
    return BasicAuth::hashPassword(password);
}

bool Geruest::removeBasicAuthUser(const std::string& username) {
    bool removed = serverData.getBasicAuth().removeUser(username);
    if (removed) {
        sendToLogger("Removed Basic Auth user: " + username);
    }
    return removed;
}

void Geruest::addProtectedPage(const std::string& path) {
    serverData.getBasicAuth().addProtectedPage(path);
    sendToLogger("Added protected page: " + path);
}

bool Geruest::removeProtectedPage(const std::string& path) {
    bool removed = serverData.getBasicAuth().removeProtectedPage(path);
    if (removed) {
        sendToLogger("Removed protected page: " + path);
    }
    return removed;
}

void Geruest::clearBasicAuthUsers() {
    serverData.getBasicAuth().clearUsers();
    sendToLogger("Cleared all Basic Auth users");
}

void Geruest::clearProtectedPages() {
    serverData.getBasicAuth().clearProtectedPages();
    sendToLogger("Cleared all protected pages");
}

// ========== Logging Configuration Implementation ==========

void Geruest::setLogLevel(LogLevel level) {
    serverData.setLogLevel(level);
    
    // Only log the change if we're at Info level or higher
    if (serverData.shouldLog(LogLevel::Info)) {
        std::string levelStr;
        switch (level) {
            case LogLevel::None: levelStr = "None"; break;
            case LogLevel::Error: levelStr = "Error"; break;
            case LogLevel::Warning: levelStr = "Warning"; break;
            case LogLevel::Info: levelStr = "Info"; break;
            case LogLevel::Debug: levelStr = "Debug"; break;
        }
        sendToLogger("Log level set to: " + levelStr);
    }
}

LogLevel Geruest::getLogLevel() const {
    return serverData.getLogLevel();
}

void Geruest::init() {
    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (this->server_fd == INVALID_SOCKET) {
        sendToLoggerError("Socket creation failed: " + std::to_string(WSAGetLastError()));
#else
    if (this->server_fd < 0) {
        sendToLoggerError("Socket creation failed");
#endif
        exit(EXIT_FAILURE);
    }

    this->address.sin_family = AF_INET;
    this->address.sin_addr.s_addr = INADDR_ANY;
    this->address.sin_port = htons((unsigned short)port);

    if (bind(this->server_fd, (struct sockaddr*)&this->address, sizeof(this->address)) < 0) {
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
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout) < 0) {
        sendToLoggerError("Failed to set receive timeout");
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

    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, (const char*)&receive_buffer_size, sizeof(receive_buffer_size)) <
        0) {
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

    running = true;

    // Start worker threads
    startWorkers();

    sendToLogger("Waiting for connections...");
    sendToLogger("Worker threads: " + std::to_string(_workerThreadCount) +
                 ", Max queue size: " + std::to_string(_maxQueueSize));

    while (running) {
        try {
#ifdef _WIN32
            SOCKET new_socket = accept(this->server_fd, (struct sockaddr*)&this->address, &addrlen);

            if (new_socket == INVALID_SOCKET) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
#else
            int new_socket = accept(this->server_fd, (struct sockaddr*)&this->address, (socklen_t*)&addrlen);

            if (new_socket < 0) {
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

            // Producer: Add connection to queue
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                if (_connectionQueue.size() >= _maxQueueSize) {
                    sendToLoggerError("Connection queue full (" + std::to_string(_maxQueueSize) +
                                      "), rejecting connection from " + client_ip_str);
#ifdef _WIN32
                    closesocket(new_socket);
#else
                    close(new_socket);
#endif
                    continue;
                }
                _connectionQueue.push({new_socket, client_ip_str});
            }
            _queueCV.notify_one();  // Wake up a worker thread

        } catch (const std::exception& e) {
            sendToLoggerError(std::string("Exception in server loop: ") + e.what());
            continue;
        }
    }

    // Stop workers before exiting
    stopWorkers();

    sendToLogger("Server stopped.");
}

#ifdef _WIN32
void Geruest::giveToHandler(SOCKET new_socket, std::string& IP) {
#else
void Geruest::giveToHandler(int new_socket, std::string& IP) {
#endif
    auto clientHandler = std::make_unique<Handler>(new_socket, IP, serverData);
    // sendToLogger("New connection");

    std::thread clientThread([handler = std::move(clientHandler), this]() mutable {
        try {
            handler->run();
        } catch (const std::exception& e) {
            sendToLoggerError(std::string("Handler error: ") + e.what());
        } catch (...) {
            sendToLoggerError("Handler encountered an unknown error");
        }
    });

    clientThread.detach();
}

void Geruest::stop() {
    sendToLogger("Stopping server at next opportunity.");

    running = false;
}

bool Geruest::isRunning() { return running; }

void Geruest::startWorkers() {
    _workersRunning = true;
    _workerThreads.reserve(_workerThreadCount);

    for (size_t i = 0; i < _workerThreadCount; ++i) {
        _workerThreads.emplace_back(&Geruest::workerThread, this);
    }

    sendToLogger("Started " + std::to_string(_workerThreadCount) + " worker threads");
}

void Geruest::stopWorkers() {
    if (!_workersRunning) {
        return;
    }

    _workersRunning = false;
    _queueCV.notify_all();  // Wake up all workers

    // Wait for all workers to finish
    for (auto& worker : _workerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    _workerThreads.clear();

    // Clear any remaining connections in queue
    std::lock_guard<std::mutex> lock(_queueMutex);
    while (!_connectionQueue.empty()) {
        auto connection = _connectionQueue.front();
        _connectionQueue.pop();
#ifdef _WIN32
        closesocket(connection.first);
#else
        close(connection.first);
#endif
    }
    
    sendToLogger("All worker threads stopped");
}

void Geruest::workerThread() {
    while (_workersRunning) {
#ifdef _WIN32
        SOCKET clientSocket;
#else
        int clientSocket;
#endif
        std::string clientIP;

        // Consumer: Get connection from queue
        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _queueCV.wait(lock, [this] { return !_connectionQueue.empty() || !_workersRunning; });

            if (!_workersRunning && _connectionQueue.empty()) {
                break;
            }

            if (_connectionQueue.empty()) {
                continue;
            }

            auto connection = _connectionQueue.front();
            _connectionQueue.pop();
            clientSocket = connection.first;
            clientIP = connection.second;
        }

        // Process the connection
        giveToHandler(clientSocket, clientIP);
    }
}

void Geruest::sendToLogger(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Info)) {
        std::cout << message << std::endl;
    }
}

void Geruest::sendToLoggerError(const std::string& message) const {
    if (serverData.shouldLog(LogLevel::Error)) {
        std::cerr << "Error: " << message << std::endl;
    }
}

}  // namespace geruest
