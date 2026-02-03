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
     * @brief Enable or disable automatic CSS/JS asset merging per page.
     * 
     * When enabled (true), the HTMLBuilder scans each HTML template for:
     * - <link rel="stylesheet" href="..."> tags
     * - <script src="..."></script> tags
     * 
     * It then:
     * 1. Extracts all referenced CSS and JS files
     * 2. Merges them into single files (page_name.css, page_name.js)
     * 3. Replaces the original tags with single includes
     * 
     * When disabled (false), assets are served individually as specified in HTML.
     * 
     * Benefits of merging:
     * - Reduces network requests per page (1 CSS + 1 JS instead of many)
     * - Automatically updates when HTML templates change
     * - No manual configuration files needed
     * 
     * @param enabled true to merge assets, false to serve individually (default: false)
     * @note Must be called before init() or start()
     */
    void setMergeAssets(bool enabled);

    /**
     * @brief Enable or disable automatic PNG/JPG to WebP conversion.
     * 
     * When enabled, the HTMLBuilder scans each HTML template for:
     * - <img src="..."> tags with .png, .jpg, .jpeg extensions
     * - CSS url() references with .png, .jpg, .jpeg extensions
     * 
     * It then:
     * 1. Extracts all referenced image paths
     * 2. Converts them to WebP format (using libwebp)
     * 3. Replaces the original references with .webp extensions
     * 
     * Behavior depends on mode:
     * - Dev mode: Images are converted on-the-fly and cached in memory
     *             (never saved to disk, regenerated each restart)
     * - Production: Converted images are saved to disk for efficiency
     * 
     * Benefits of WebP conversion:
     * - Significantly smaller file sizes (25-35% smaller than PNG/JPG)
     * - Faster page load times
     * - Automatic format optimization
     * 
     * @param enabled true to convert images to WebP, false to serve original formats
     * @note Must be called before init() or start()
     * @note Requires libwebp library for WebP encoding
     */
    void setWebPConversion(bool enabled);

    /**
     * @brief Enable automatic WebP conversion (alias for setWebPConversion(true))
     * 
     * Convenience method that follows the same pattern as enableDevMode().
     * @see setWebPConversion for detailed behavior description
     */
    void enableWebPConversion();

    /**
     * @brief Set WebP encoding quality.
     * 
     * Controls the quality/size tradeoff for WebP image conversion.
     * Higher values produce better quality images but larger file sizes.
     * 
     * @param quality Quality value from 0-100 (default: 75)
     *        - 0-50: Low quality, very small files (thumbnails, previews)
     *        - 50-70: Medium quality, good for web backgrounds
     *        - 70-85: High quality, recommended for most images (default: 75)
     *        - 85-100: Near-lossless, for images requiring high fidelity
     * 
     * @note Values outside 0-100 are clamped to the valid range
     * @note Must be called before init() or start() to affect initial conversion
     */
    void setWebPQuality(float quality);

    /**
     * @brief Enable development mode for easier debugging and rapid development.
     * 
     * When enabled, development mode:
     * - Sets log level to Debug (shows all logs including verbose information)
     * - Disables file caching (content generated in-memory only)
     * - Keeps HTML/CSS/JS comments (easier debugging)
     * 
     * This is particularly useful during development when HTML, CSS, and JS files
     * change frequently, as the generated/merged files won't be saved to disk.
     * Files are regenerated on each request for immediate feedback.
     * 
     * @note Development mode is disabled by default
     * @note Should be disabled in production for better performance
     * @note Must be called before init() or start()
     */
    void enableDevMode();

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

    // ========== Basic Authentication Methods ==========
    
    /**
     * @brief Enable or disable Basic Authentication globally
     * @param enabled true to enable authentication, false to disable
     * @note When disabled, all pages are accessible without credentials
     */
    void setBasicAuthEnabled(bool enabled);
    
    /**
     * @brief Add a user with credentials for Basic Authentication
     * @param username The username
     * @param password The password (will be hashed with SHA-256 before storage)
     * @note Authentication must be enabled and users must be added for protection to work
     */
    void addBasicAuthUser(const std::string& username, const std::string& password);
    
    /**
     * @brief Add a user with pre-hashed password for Basic Authentication
     * @param username The username
     * @param hashedPassword SHA-256 hash of the password (64 hex characters)
     * @note Use this when you want to store pre-computed password hashes
     */
    void addBasicAuthUserHashed(const std::string& username, const std::string& hashedPassword);
    
    /**
     * @brief Generate SHA-256 hash from a plain text password
     * @param password Plain text password
     * @return SHA-256 hash as hexadecimal string (64 characters)
     * @note Useful for generating hashes to store in configuration files
     */
    static std::string hashPassword(const std::string& password);
    
    /**
     * @brief Remove a user from Basic Authentication
     * @param username The username to remove
     * @return true if user was removed, false if not found
     */
    bool removeBasicAuthUser(const std::string& username);
    
    /**
     * @brief Add a page to the protected pages list (requires authentication)
     * @param path The path to protect (e.g., "/admin", "/api/admin")
     * @note Page will only be protected if authentication is enabled and users exist
     */
    void addProtectedPage(const std::string& path);
    
    /**
     * @brief Remove a page from protected pages list
     * @param path The path to unprotect
     * @return true if page was removed, false if not found
     */
    bool removeProtectedPage(const std::string& path);
    
    /**
     * @brief Clear all Basic Authentication users
     */
    void clearBasicAuthUsers();
    
    /**
     * @brief Clear all protected pages
     */
    void clearProtectedPages();

    // ========== Logging Configuration Methods ==========

    /**
     * @brief Set the log level for filtering log output
     * @param level LogLevel enum value (None, Error, Warning, Info, Debug)
     * @note Default is Error. Set before init() or during runtime
     * 
     * Levels:
     * - None: No logging
     * - Error: Only critical errors (default)
     * - Warning: Errors and warnings (recommended for production)
     * - Info: Errors, warnings, and informational messages
     * - Debug: All messages including verbose debug information
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Get the current log level
     * @return Current LogLevel
     */
    LogLevel getLogLevel() const;

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
