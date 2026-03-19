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
#include "config/ConfigLoader.hpp"
#if GERUEST_HAS_CURL
#include "email/EmailSender.hpp"
#endif

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

    /**
     * @brief Add a redirect from one route to another route or URL.
     * @param from Source route pattern (supports '*' wildcard)
     * @param to Target route or external URL. If it contains '*', the wildcard capture is forwarded.
     * @param status Redirect status code (301 or 302, default: 301)
     */
    void addRedirect(const std::string& from, const std::string& to, int status = 301);

    /**
     * @brief Add multiple redirects with a shared status code.
     * @param redirects Mapping of source route pattern to target route/URL
     * @param status Redirect status code (301 or 302, default: 301)
     */
    void addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status = 301);

    void addRoot(const std::string& root);

    /**
     * @brief Set a custom page path for 404 responses (e.g. "/404.html").
     * @note Should be called before init() or start()
     */
    void set404(const std::string& path);

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
     * @brief Enable automatic CSS/JS asset merging (alias for setMergeAssets(true))
     * 
     * Convenience method that follows the same pattern as enableDevMode().
     * @see setMergeAssets for detailed behavior description
     */
    void enableMergeAssets();

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
     * @brief Set JavaScript obfuscation level
     * 
     * Controls the level of obfuscation applied to JavaScript files:
     * - Level 0: Disabled (default) - no obfuscation
     * - Level 1: Basic - variable/function name mangling + whitespace removal
     * - Level 2: Medium - Level 1 + string encoding + number obfuscation
     * - Level 3: Advanced - Level 2 + dead code injection + control flow obfuscation
     * 
     * Obfuscation behavior:
     * - Only applies when dev mode is OFF (dev mode disables obfuscation)
     * - Obfuscated files are cached on disk for performance
     * - Cache respects expiry time (default: 7 days)
     * - Excluded files (via addObfuscationExclusion) are never obfuscated or merged
     * 
     * @param level Obfuscation level (0-3, default: 0)
     * @note Must be called before init() or start()
     */
    void setObfuscationLevel(unsigned int level);

    /**
     * @brief Set cache expiry time for obfuscated JavaScript files
     * 
     * Determines how long obfuscated JS files are kept on disk before
     * being regenerated. Uses file modification time for checking.
     * 
     * @param days Number of days to cache obfuscated files (default: 7)
     * @note Applies only when obfuscation level > 0 and dev mode is off
     */
    void setObfuscationCacheExpiry(int days);

    /**
     * @brief Exclude a JavaScript file from obfuscation and merging
     * 
     * Files added to the exclusion list will:
     * - NOT be obfuscated (served as-is)
     * - NOT be merged with other JS files
     * - Be served individually when requested
     * 
     * Use this for:
     * - External libraries (jquery.min.js, bootstrap.min.js, etc.)
     * - Already minified/obfuscated code
     * - Third-party scripts that might break if modified
     * 
     * @param filename Exact filename to exclude (e.g., "jquery.min.js")
     * @note Matching is exact - filename must match exactly
     */
    void addObfuscationExclusion(const std::string& filename);

    /**
     * @brief Load configuration from .env file and environment variables
     * 
     * This method loads configuration values following the hierarchy:
     * 1. Code (explicit setter calls) - highest priority, never overwritten
     * 2. .env file values - middle priority
     * 3. Environment variables (getenv) - lowest priority
     * 
     * Supported configuration keys:
     * - PORT (int): Server port (default: 8080)
     * - HOSTNAME (string): Server hostname (default: "localhost")
     * - WEBP_CONVERSION (bool): Enable WebP conversion (default: false)
     * - WEBP_QUALITY (float): WebP quality 0-100 (default: 75)
     * - DEV_MODE (bool): Enable development mode (default: false)
     * - MERGE_ASSETS (bool): Enable asset merging (default: false)
     * - WORKER_THREADS (size_t): Number of worker threads (default: CPU cores * 2)
     * - MAX_QUEUE_SIZE (size_t): Maximum connection queue size (default: 500)
     * - LOG_LEVEL (string): Log level: "none", "error", "warning", "info", "debug" (default: "error")
     * 
     * Email Configuration:
     * - SMTP_SERVER (string): SMTP server hostname
     * - SMTP_PORT (int): SMTP port (default: 587)
     * - SMTP_USERNAME (string): SMTP username
     * - SMTP_PASSWORD (string): SMTP password
     * - SMTP_FROM_ADDRESS (string): Email from address
     * - SMTP_USE_TLS (bool): Enable TLS (default: true)
     * - EMAIL_MIN_INTERVAL (int): Min seconds between emails per IP (default: 60)
     * - EMAIL_MAX_PER_IP (size_t): Max emails per IP (default: 10)
     * - EMAIL_TRACKING_DURATION (int): IP tracking duration in seconds (default: 3600)
     * - EMAIL_MAX_QUEUE_SIZE (size_t): Max email queue size (default: 1000)
     * 
     * @param envFilePath Path to .env file (default: ".env")
     * @note Call this before init() or start()
     * @note Only values NOT explicitly set via setters will be loaded from config
     */
    void loadConfig(const std::string& envFilePath = ".env");

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
    void setBasicAuth(bool enabled);

    /**
     * @brief Enable Basic Authentication (alias for setBasicAuth(true))
     * 
     * Convenience method that follows the same pattern as enableDevMode().
     * @see setBasicAuth for detailed behavior description
     */
    void enableBasicAuth();
    
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

    // ========== Email Configuration Methods ==========
#if GERUEST_HAS_CURL    /**
     * @brief Initialize the email sender with SMTP configuration
     * @param smtpServer SMTP server hostname (e.g., "smtp.gmail.com")
     * @param smtpPort SMTP port (default: 587 for TLS)
     * @param username SMTP username
     * @param password SMTP password (for Gmail, use app-specific password)
     * @param fromAddress Email address to send from
     * @param useTLS Enable TLS encryption (default: true)
     * @note Must be called before using email functionality
     */
    void initEmail(const std::string& smtpServer, int smtpPort,
                   const std::string& username, const std::string& password,
                   const std::string& fromAddress, bool useTLS = true);

    /**
     * @brief Set minimum time interval between emails from same IP
     * @param seconds Minimum seconds between emails (default: 60)
     */
    void setEmailMinInterval(int seconds);

    /**
     * @brief Set maximum number of emails per IP in tracking window
     * @param count Maximum emails allowed (default: 10)
     */
    void setEmailMaxPerIP(size_t count);

    /**
     * @brief Set how long to track IP activity for spam protection
     * @param seconds Seconds to track IP (default: 3600 = 1 hour)
     */
    void setEmailTrackingDuration(int seconds);

    /**
     * @brief Set maximum email queue size
     * @param size Maximum pending emails (default: 1000)
     */
    void setEmailMaxQueueSize(size_t size);
#endif  // GERUEST_HAS_CURL

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

    /**
     * @brief Activate the /status metrics endpoint (token-protected).
     *
     * The endpoint returns a JSON snapshot of server health and metrics.
     * Access requires the HTTP header: Authorization: Bearer <token>
     * In dev mode, the token check is skipped and the endpoint is openly accessible.
     *
     * Response fields:
     * - health: "ok" | "degraded" | "overloaded"
     * - version: Geruest framework version (useful for multi-version comparisons)
     * - timestamp: ISO 8601 UTC time of the snapshot
     * - uptime_seconds: seconds since server start
     * - requests.total / last_hour / avg_per_hour / active
     * - errors.total / client_4xx / server_5xx / internal (+ last_hour/avg_per_hour breakdown)
     * - queue.current_size / max_size / rejections_total / avg_fill_percent_hour / avg_fill_percent_per_hour
     * - latency_ms.p50 / p95 / p99 (milliseconds, last 60 seconds)
     * - system.memory.total_mb / used_mb / free_mb / percent_used (host memory)
     * - system.cpu.count (logical CPU cores)
     * - system.cpu.load_1m / load_5m / load_15m (system-wide, all cores combined; Linux only, 0 on Windows)
     *     Normalize by count to get per-core utilization: load_1m / count * 100 ≈ CPU %
     * - system.disk.total_gb / used_gb / free_gb / percent_used (root "/" on Linux, "C:\" on Windows)
     * - system.cgroup_memory.limit_mb / used_mb / free_mb / percent_used
     *     (only present when a cgroup memory limit is detected, e.g. inside Docker)
     *
     * Health thresholds:
     * - degraded:   avg queue fill (last hour) >= 50% OR requests (last hour) >= 500
     * - overloaded: avg queue fill (last hour) >= 80% OR requests (last hour) >= 1000
     *
     * @param token Bearer token required in the Authorization header.
     * @note Should be called before start().
     */
    void enableStatus(const std::string& token);

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

    bool _statusActive = false;
    std::string _statusToken;

    ServerData serverData;

    // Thread pool configuration
    size_t _workerThreadCount = std::thread::hardware_concurrency() * 2;
    size_t _maxQueueSize = 500;

    // Configuration flags to track values set explicitly via code
    // Values set via code take precedence over .env and environment variables
    struct ConfigFlags {
        bool portSet = false;
        bool hostnameSet = false;
        bool webpConversionSet = false;
        bool webpQualitySet = false;
        bool devModeSet = false;
        bool mergeAssetsSet = false;
        bool workerThreadsSet = false;
        bool maxQueueSizeSet = false;
        bool logLevelSet = false;
        
#if GERUEST_HAS_CURL
        // Email configuration flags
        bool emailInitialized = false;
        bool emailMinIntervalSet = false;
        bool emailMaxPerIPSet = false;
        bool emailTrackingDurationSet = false;
        bool emailMaxQueueSizeSet = false;
#endif
    } _configFlags;

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
    std::atomic<size_t> _queueSize{0};  // mirror of _connectionQueue.size() for lock-free status reporting

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
