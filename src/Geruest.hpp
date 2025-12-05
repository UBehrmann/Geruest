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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  // For close
#endif

#include <atomic>
#include <condition_variable>
#include <cstring>  // For memset
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"
#include "handler/Handler.hpp"
#include "parser/JSONParser.hpp"

// Constants
#define TIMEOUT_SEC 30
#define TIMEOUT_USEC 0

// Max packet size
#define BUFFER_SIZE 8192

namespace geruest {

class Geruest {
   public:
    Geruest();
    ~Geruest();

    void setPort(int port);

    void setHostname(const std::string& hostname);

    void addRoute(const std::string& path, RouteHandler handler);

    void addRoot(const std::string& root);

    /**
     * @brief Sets the available languages for the server.
     * @param languages Vector of language codes (e.g., {"en", "de", "fr"})
     * @note The first language will be used as the default. If empty, no language routing.
     * @note Must be called before init() or start()
     */
    void setAvailableLanguages(const std::vector<std::string>& languages);

    /**
     * @brief Sets the number of worker threads in the thread pool.
     * @param count Number of worker threads (default: CPU cores * 2)
     * @note Must be called before init() or start()
     */
    void setWorkerThreadCount(size_t count);

    /**
     * @brief Sets the maximum size of the connection queue.
     * @param size Maximum number of pending connections (default: 500)
     * @note Must be called before init() or start()
     */
    void setMaxQueueSize(size_t size);

    /*
     * Initializes the server, sets up the socket, binds it to the address and port,
     * and prepares it to listen for incoming connections.
     * This method should be called before starting the server.
     */
    void init();

    void start();

    void stop();

    /**
     * @brief Checks if the server is currently running.
     * @return true if the server is running, false otherwise.
     */
    bool isRunning();

   private:
#ifdef _WIN32
    SOCKET server_fd = INVALID_SOCKET;  // Socket descriptor for the server
#else
    int server_fd = -1;  // Socket descriptor for the server
#endif

    struct sockaddr_in address{};

    bool running = false;

    int port = 8080;

    std::string hostname_ = "localhost";

    ServerData serverData;

    // Thread pool configuration
    size_t _workerThreadCount = std::thread::hardware_concurrency() * 2;
    size_t _maxQueueSize = 500;

    // Thread pool components
    std::vector<std::thread> _workerThreads;
    std::queue<std::pair<
#ifdef _WIN32
        SOCKET,
#else
        int,
#endif
        std::string>>
        _connectionQueue;
    std::mutex _queueMutex;
    std::condition_variable _queueCV;
    std::atomic<bool> _workersRunning{false};

    void sendToLogger(const std::string& message) const;

    void sendToLoggerError(const std::string& message) const;

    /**
     * @brief Worker thread function that processes connections from the queue.
     */
    void workerThread();

    /**
     * @brief Starts the worker thread pool.
     */
    void startWorkers();

    /**
     * @brief Stops the worker thread pool and waits for all threads to finish.
     */
    void stopWorkers();

#ifdef _WIN32
    void giveToHandler(SOCKET new_socket, std::string& IP);
#else
    void giveToHandler(int new_socket, std::string& IP);
#endif
};

}  // namespace geruest

#endif  // GERUEST_GERUEST_HPP
