/**
 * @file server/Socket.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Socket setup and main accept loop (init, start, stop, isRunning).
 */

#include "../Geruest.hpp"

#include <chrono>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

namespace geruest {

void Geruest::statusPersistenceLoop() {
    while (running) {
        for (int i = 0; i < 3600 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running) {
            break;
        }
        if (!serverData.savePersistentMetricsToFile(_statusPersistencePath)) {
            sendToLoggerError("Status metrics persistence: periodic save failed for " + _statusPersistencePath);
        }
    }
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

    this->address.sin_family      = AF_INET;
    this->address.sin_addr.s_addr = INADDR_ANY;
    this->address.sin_port        = htons(static_cast<unsigned short>(port));

    if (bind(this->server_fd, reinterpret_cast<struct sockaddr*>(&this->address), sizeof(this->address)) < 0) {
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

#ifdef _WIN32
    DWORD timeout = TIMEOUT_SEC * 1000;
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
        sendToLoggerError("Failed to set receive timeout: " + std::to_string(WSAGetLastError()));
#else
    struct timeval timeout;
    timeout.tv_sec  = TIMEOUT_SEC;
    timeout.tv_usec = TIMEOUT_USEC;
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) < 0) {
        sendToLoggerError("Failed to set receive timeout");
#endif
        exit(EXIT_FAILURE);
    }

    int send_buffer_size    = BUFFER_SIZE;
    int receive_buffer_size = BUFFER_SIZE;

    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&send_buffer_size), sizeof(send_buffer_size)) < 0) {
#ifdef _WIN32
        sendToLoggerError("Failed to set send buffer size: " + std::to_string(WSAGetLastError()));
#else
        sendToLoggerError("Failed to set send buffer size");
#endif
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&receive_buffer_size), sizeof(receive_buffer_size)) < 0) {
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
    if (!serverData.loadPersistentMetricsFromFile(_statusPersistencePath)) {
        sendToLoggerError("Status metrics persistence: invalid or unsupported file " + _statusPersistencePath);
    }

    int addrlen = sizeof(this->address);

    running = true;
    _statusPersistenceThread = std::thread(&Geruest::statusPersistenceLoop, this);
    startWorkers();

    sendToLogger("Waiting for connections...");
    sendToLogger("Worker threads: " + std::to_string(_workerThreadCount) +
                 ", Max queue size: " + std::to_string(_maxQueueSize));

    while (running) {
        try {
#ifdef _WIN32
            SOCKET new_socket = accept(this->server_fd, reinterpret_cast<struct sockaddr*>(&this->address), &addrlen);
            if (new_socket == INVALID_SOCKET) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
#else
            int new_socket = accept(this->server_fd, reinterpret_cast<struct sockaddr*>(&this->address), reinterpret_cast<socklen_t*>(&addrlen));
            if (new_socket < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
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

            char client_ip[INET_ADDRSTRLEN];
#ifdef _WIN32
            strcpy(client_ip, inet_ntoa(address.sin_addr));
#else
            inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
#endif
            std::string client_ip_str(client_ip);

            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                if (_connectionQueue.size() >= _maxQueueSize) {
                    serverData.recordQueueRejection();
                    serverData.recordQueueFill(100.0f);
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
                _queueSize.fetch_add(1, std::memory_order_relaxed);
                if (_maxQueueSize > 0) {
                    const float fill = 100.0f * static_cast<float>(_queueSize.load(std::memory_order_relaxed)) /
                                       static_cast<float>(_maxQueueSize);
                    serverData.recordQueueFill(fill > 100.0f ? 100.0f : (fill < 0.0f ? 0.0f : fill));
                }
            }
            _queueCV.notify_one();

        } catch (const std::exception& e) {
            sendToLoggerError(std::string("Exception in server loop: ") + e.what());
            continue;
        }
    }

    stopWorkers();
    if (_statusPersistenceThread.joinable()) {
        _statusPersistenceThread.join();
    }
    if (!serverData.savePersistentMetricsToFile(_statusPersistencePath)) {
        sendToLoggerError("Status metrics persistence: shutdown save failed for " + _statusPersistencePath);
    }
    sendToLogger("Server stopped.");
}

void Geruest::stop() {
    sendToLogger("Stopping server at next opportunity.");
    running = false;
}

bool Geruest::isRunning() { return running; }

}  // namespace geruest
