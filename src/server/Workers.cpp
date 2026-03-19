/**
 * @file server/Workers.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Thread pool implementation (startWorkers, stopWorkers, workerThread, giveToHandler).
 */

#include "../Geruest.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

namespace geruest {

void Geruest::startWorkers() {
    _workersRunning = true;
    _workerThreads.reserve(_workerThreadCount);

    for (size_t i = 0; i < _workerThreadCount; ++i) {
        _workerThreads.emplace_back(&Geruest::workerThread, this);
    }

    sendToLogger("Started " + std::to_string(_workerThreadCount) + " worker threads");
}

void Geruest::stopWorkers() {
    if (!_workersRunning) return;

    _workersRunning = false;
    _queueCV.notify_all();

    for (auto& worker : _workerThreads) {
        if (worker.joinable()) worker.join();
    }
    _workerThreads.clear();

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
    _queueSize.store(0, std::memory_order_relaxed);

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

        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _queueCV.wait(lock, [this] { return !_connectionQueue.empty() || !_workersRunning; });

            if (!_workersRunning && _connectionQueue.empty()) break;
            if (_connectionQueue.empty()) continue;

            auto connection = _connectionQueue.front();
            _connectionQueue.pop();
            _queueSize.fetch_sub(1, std::memory_order_relaxed);
            if (_maxQueueSize > 0) {
                const float fill = 100.0f * static_cast<float>(_queueSize.load(std::memory_order_relaxed)) /
                                   static_cast<float>(_maxQueueSize);
                serverData.recordQueueFill(fill > 100.0f ? 100.0f : (fill < 0.0f ? 0.0f : fill));
            }
            clientSocket = connection.first;
            clientIP     = connection.second;
        }

        giveToHandler(clientSocket, clientIP);
    }
}

#ifdef _WIN32
void Geruest::giveToHandler(SOCKET new_socket, std::string& IP) {
#else
void Geruest::giveToHandler(int new_socket, std::string& IP) {
#endif
    auto clientHandler = std::make_unique<Handler>(new_socket, IP, serverData);

    std::thread clientThread([handler = std::move(clientHandler), this]() mutable {
        try {
            handler->run();
        } catch (const std::exception& e) {
            serverData.recordError();
            sendToLoggerError(std::string("Handler error: ") + e.what());
        } catch (...) {
            serverData.recordError();
            sendToLoggerError("Handler encountered an unknown error");
        }
    });

    clientThread.detach();
}

}  // namespace geruest
