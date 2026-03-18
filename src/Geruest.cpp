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

#include <algorithm>
#include <ctime>
#include <iostream>
#include <sstream>
#include <thread>

#include "data/HTTPResponse.hpp"
#include "geruest/Version.hpp"
#include "parser/JSONParser.hpp"

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

void Geruest::setPort(int _port) { 
    port = _port; 
    _configFlags.portSet = true;
}

void Geruest::setHostname(const std::string& hostname) { 
    hostname_ = hostname; 
    _configFlags.hostnameSet = true;
}

void Geruest::addRoute(const std::string& path, RouteHandler routeHandler) {
    serverData.addRoute(path, std::move(routeHandler));
}

void Geruest::addRedirect(const std::string& from, const std::string& to, int status) {
    if (serverData.addRedirect(from, to, status)) {
        sendToLogger("Added redirect: " + from + " -> " + to + " (" + std::to_string(status) + ")");
    } else {
        sendToLoggerError("Skipped redirect (invalid or loop detected): " + from + " -> " + to);
    }
}

void Geruest::addRedirects(const std::unordered_map<std::string, std::string>& redirects, int status) {
    const size_t addedCount = serverData.addRedirects(redirects, status);
    sendToLogger("Added redirect map entries: " + std::to_string(addedCount) + "/" + std::to_string(redirects.size())
                 + " (" + std::to_string(status) + ")");
}

void Geruest::addRoot(const std::string& root) { serverData.setRoot(root); }

void Geruest::set404(const std::string& path) {
    serverData.setNotFoundPage(path);
    sendToLogger("Custom 404 page set to: " + path);
}

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
    _configFlags.mergeAssetsSet = true;
    sendToLogger(std::string("Asset merging ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::enableMergeAssets() { setMergeAssets(true); }

void Geruest::setWebPConversion(bool enabled) {
#if GERUEST_HAS_WEBP
    serverData.setWebPConversion(enabled);
    _configFlags.webpConversionSet = true;
    sendToLogger(std::string("WebP conversion ") + (enabled ? "enabled" : "disabled"));
#else
    if (enabled) {
        sendToLoggerError("WebP conversion cannot be enabled - library not available (GERUEST_HAS_WEBP=0)");
        sendToLoggerError("Install libwebp-dev (apt) or webp (vcpkg) and rebuild to enable this feature");
    }
    serverData.setWebPConversion(false);
#endif
}

void Geruest::enableWebPConversion() {
#if GERUEST_HAS_WEBP
    serverData.enableWebPConversion();
    _configFlags.webpConversionSet = true;
    sendToLogger("WebP conversion enabled");
#else
    sendToLoggerError("WebP conversion cannot be enabled - library not available (GERUEST_HAS_WEBP=0)");
    sendToLoggerError("Install libwebp-dev (apt) or webp (vcpkg) and rebuild to enable this feature");
    serverData.setWebPConversion(false);
#endif
}

void Geruest::setWebPQuality(float quality) {
#if GERUEST_HAS_WEBP
    serverData.setWebPQuality(quality);
    _configFlags.webpQualitySet = true;
    std::ostringstream oss;
    oss << "WebP quality set to " << quality << "%";
    sendToLogger(oss.str());
#else
    sendToLoggerError("WebP quality cannot be set - WebP conversion not available (GERUEST_HAS_WEBP=0)");
#endif
}

void Geruest::setObfuscationLevel(unsigned int level) {
    serverData.setObfuscationLevel(level);
    if (level > 0) {
        sendToLogger("JS obfuscation enabled (level " + std::to_string(level) + ")");
    } else {
        sendToLogger("JS obfuscation disabled");
    }
}

void Geruest::setObfuscationCacheExpiry(int days) {
    serverData.setObfuscationCacheExpiry(days);
    sendToLogger("Obfuscation cache expiry set to " + std::to_string(days) + " days");
}

void Geruest::addObfuscationExclusion(const std::string& filename) {
    serverData.addObfuscationExclusion(filename);
    sendToLogger("Added obfuscation exclusion: " + filename);
}

void Geruest::enableDevMode() {
    serverData.enableDevMode();
    _configFlags.devModeSet = true;
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
        _configFlags.workerThreadsSet = true;
        return;
    }
    _workerThreadCount = count;
    _configFlags.workerThreadsSet = true;
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
        _configFlags.maxQueueSizeSet = true;
        return;
    }
    _maxQueueSize = size;
    _configFlags.maxQueueSizeSet = true;
    sendToLogger("Maximum queue size set to: " + std::to_string(_maxQueueSize));
}

// ========== Basic Authentication Implementation ==========

void Geruest::setBasicAuth(bool enabled) {
    serverData.getBasicAuth().setEnabled(enabled);
    sendToLogger(std::string("Basic Authentication ") + (enabled ? "enabled" : "disabled"));
}

void Geruest::enableBasicAuth() { setBasicAuth(true); }

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

#if GERUEST_HAS_CURL
// ========== Email Configuration Implementation ==========

void Geruest::initEmail(const std::string& smtpServer, int smtpPort,
                        const std::string& username, const std::string& password,
                        const std::string& fromAddress, bool useTLS) {
    EmailSender::Config config;
    config.smtpServer = smtpServer;
    config.port = smtpPort;
    config.username = username;
    config.password = password;
    config.fromAddress = fromAddress;
    config.useTLS = useTLS;
    
    EmailSender::init(config);
    _configFlags.emailInitialized = true;
    sendToLogger("Email sender initialized: " + smtpServer + ":" + std::to_string(smtpPort));
}

void Geruest::setEmailMinInterval(int seconds) {
    try {
        auto& emailSender = EmailSender::getInstance();
        emailSender.setMinEmailInterval(seconds);
        _configFlags.emailMinIntervalSet = true;
        sendToLogger("Email min interval set to: " + std::to_string(seconds) + "s");
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set email interval - email sender not initialized");
    }
}

void Geruest::setEmailMaxPerIP(size_t count) {
    try {
        auto& emailSender = EmailSender::getInstance();
        emailSender.setMaxEmailsPerIP(count);
        _configFlags.emailMaxPerIPSet = true;
        sendToLogger("Email max per IP set to: " + std::to_string(count));
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set max emails per IP - email sender not initialized");
    }
}

void Geruest::setEmailTrackingDuration(int seconds) {
    try {
        auto& emailSender = EmailSender::getInstance();
        emailSender.setIPTrackingDuration(seconds);
        _configFlags.emailTrackingDurationSet = true;
        sendToLogger("Email tracking duration set to: " + std::to_string(seconds) + "s");
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set tracking duration - email sender not initialized");
    }
}

void Geruest::setEmailMaxQueueSize(size_t size) {
    try {
        auto& emailSender = EmailSender::getInstance();
        emailSender.setMaxQueueSize(size);
        _configFlags.emailMaxQueueSizeSet = true;
        sendToLogger("Email max queue size set to: " + std::to_string(size));
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set email queue size - email sender not initialized");
    }
}
#endif  // GERUEST_HAS_CURL

// ========== Logging Configuration Implementation ==========

void Geruest::setLogLevel(LogLevel level) {
    serverData.setLogLevel(level);
    _configFlags.logLevelSet = true;
    
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

void Geruest::loadConfig(const std::string& envFilePath) {
    sendToLogger("Loading configuration from environment...");
    
    // Load .env file (if it exists)
    ConfigLoader::loadEnvFile(envFilePath);
    
    // Apply configuration values only if NOT explicitly set via code
    // Hierarchy: Code > .env > environment variables
    
    // PORT
    if (!_configFlags.portSet) {
        int configPort = ConfigLoader::getInt("PORT", port);
        if (configPort != port) {
            port = configPort;
            sendToLogger("PORT loaded from config: " + std::to_string(port));
        }
    }
    
    // HOSTNAME
    if (!_configFlags.hostnameSet) {
        std::string configHostname = ConfigLoader::get("HOSTNAME", hostname_);
        if (configHostname != hostname_) {
            hostname_ = configHostname;
            sendToLogger("HOSTNAME loaded from config: " + hostname_);
        }
    }
    
    // WEBP_CONVERSION
    if (!_configFlags.webpConversionSet) {
        bool configWebP = ConfigLoader::getBool("WEBP_CONVERSION", false);
        if (configWebP) {
#if GERUEST_HAS_WEBP
            serverData.setWebPConversion(true);
            sendToLogger("WEBP_CONVERSION enabled from config");
#else
            sendToLoggerError("WEBP_CONVERSION cannot be enabled - library not available");
#endif
        }
    }
    
    // WEBP_QUALITY
    if (!_configFlags.webpQualitySet) {
        float configQuality = ConfigLoader::getFloat("WEBP_QUALITY", 75.0f);
        if (configQuality >= 0.0f && configQuality <= 100.0f) {
#if GERUEST_HAS_WEBP
            serverData.setWebPQuality(configQuality);
            sendToLogger("WEBP_QUALITY loaded from config: " + std::to_string(static_cast<int>(configQuality)) + "%");
#endif
        }
    }
    
    // DEV_MODE
    if (!_configFlags.devModeSet) {
        bool configDevMode = ConfigLoader::getBool("DEV_MODE", false);
        if (configDevMode) {
            serverData.enableDevMode();
            sendToLogger("DEV_MODE enabled from config");
        }
    }
    
    // MERGE_ASSETS
    if (!_configFlags.mergeAssetsSet) {
        bool configMerge = ConfigLoader::getBool("MERGE_ASSETS", false);
        if (configMerge) {
            serverData.setMergeAssets(true);
            sendToLogger("MERGE_ASSETS enabled from config");
        }
    }
    
    // WORKER_THREADS
    if (!_configFlags.workerThreadsSet) {
        size_t configThreads = ConfigLoader::getSizeT("WORKER_THREADS", _workerThreadCount);
        if (configThreads > 0 && configThreads != _workerThreadCount) {
            _workerThreadCount = configThreads;
            sendToLogger("WORKER_THREADS loaded from config: " + std::to_string(_workerThreadCount));
        }
    }
    
    // MAX_QUEUE_SIZE
    if (!_configFlags.maxQueueSizeSet) {
        size_t configQueueSize = ConfigLoader::getSizeT("MAX_QUEUE_SIZE", _maxQueueSize);
        if (configQueueSize > 0 && configQueueSize != _maxQueueSize) {
            _maxQueueSize = configQueueSize;
            sendToLogger("MAX_QUEUE_SIZE loaded from config: " + std::to_string(_maxQueueSize));
        }
    }
    
    // LOG_LEVEL
    if (!_configFlags.logLevelSet) {
        std::string configLogLevel = ConfigLoader::get("LOG_LEVEL", "");
        if (!configLogLevel.empty()) {
            LogLevel level = LogLevel::Error;  // Default
            std::string levelLower = configLogLevel;
            std::transform(levelLower.begin(), levelLower.end(), levelLower.begin(), ::tolower);
            
            if (levelLower == "none") {
                level = LogLevel::None;
            } else if (levelLower == "error") {
                level = LogLevel::Error;
            } else if (levelLower == "warning" || levelLower == "warn") {
                level = LogLevel::Warning;
            } else if (levelLower == "info") {
                level = LogLevel::Info;
            } else if (levelLower == "debug") {
                level = LogLevel::Debug;
            } else {
                sendToLoggerError("Invalid LOG_LEVEL in config: " + configLogLevel + " (using default: Error)");
            }
            
            serverData.setLogLevel(level);
            sendToLogger("LOG_LEVEL loaded from config: " + configLogLevel);
        }
    }
    
#if GERUEST_HAS_CURL
    // ========== Email Configuration ==========
    
    // Initialize email sender if not already initialized via code
    if (!_configFlags.emailInitialized) {
        std::string smtpServer = ConfigLoader::get("SMTP_SERVER", "");
        std::string smtpUsername = ConfigLoader::get("SMTP_USERNAME", "");
        std::string smtpPassword = ConfigLoader::get("SMTP_PASSWORD", "");
        std::string smtpFromAddress = ConfigLoader::get("SMTP_FROM_ADDRESS", "");
        
        // Only initialize if we have at least server and credentials
        if (!smtpServer.empty() && !smtpUsername.empty() && !smtpPassword.empty()) {
            int smtpPort = ConfigLoader::getInt("SMTP_PORT", 587);
            bool smtpUseTLS = ConfigLoader::getBool("SMTP_USE_TLS", true);
            
            if (smtpFromAddress.empty()) {
                smtpFromAddress = smtpUsername;  // Use username as from address if not specified
            }
            
            EmailSender::Config emailConfig;
            emailConfig.smtpServer = smtpServer;
            emailConfig.port = smtpPort;
            emailConfig.username = smtpUsername;
            emailConfig.password = smtpPassword;
            emailConfig.fromAddress = smtpFromAddress;
            emailConfig.useTLS = smtpUseTLS;
            
            EmailSender::init(emailConfig);
            sendToLogger("Email sender initialized from config: " + smtpServer + ":" + std::to_string(smtpPort));
        }
    }
    
    // Email spam protection settings (only if email sender exists)
    try {
        auto& emailSender = EmailSender::getInstance();
        
        // EMAIL_MIN_INTERVAL
        if (!_configFlags.emailMinIntervalSet) {
            int configMinInterval = ConfigLoader::getInt("EMAIL_MIN_INTERVAL", 60);
            if (configMinInterval > 0) {
                emailSender.setMinEmailInterval(configMinInterval);
                sendToLogger("EMAIL_MIN_INTERVAL loaded from config: " + std::to_string(configMinInterval) + "s");
            }
        }
        
        // EMAIL_MAX_PER_IP
        if (!_configFlags.emailMaxPerIPSet) {
            size_t configMaxPerIP = ConfigLoader::getSizeT("EMAIL_MAX_PER_IP", 10);
            if (configMaxPerIP > 0) {
                emailSender.setMaxEmailsPerIP(configMaxPerIP);
                sendToLogger("EMAIL_MAX_PER_IP loaded from config: " + std::to_string(configMaxPerIP));
            }
        }
        
        // EMAIL_TRACKING_DURATION
        if (!_configFlags.emailTrackingDurationSet) {
            int configTrackingDuration = ConfigLoader::getInt("EMAIL_TRACKING_DURATION", 3600);
            if (configTrackingDuration > 0) {
                emailSender.setIPTrackingDuration(configTrackingDuration);
                sendToLogger("EMAIL_TRACKING_DURATION loaded from config: " + std::to_string(configTrackingDuration) + "s");
            }
        }
        
        // EMAIL_MAX_QUEUE_SIZE
        if (!_configFlags.emailMaxQueueSizeSet) {
            size_t configMaxQueueSize = ConfigLoader::getSizeT("EMAIL_MAX_QUEUE_SIZE", 1000);
            if (configMaxQueueSize > 0) {
                emailSender.setMaxQueueSize(configMaxQueueSize);
                sendToLogger("EMAIL_MAX_QUEUE_SIZE loaded from config: " + std::to_string(configMaxQueueSize));
            }
        }
    } catch (const std::runtime_error&) {
        // Email sender not initialized - this is fine, email functionality is optional
    }
#endif  // GERUEST_HAS_CURL
    
    sendToLogger("Configuration loading complete");
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
            serverData.recordError();
            sendToLoggerError(std::string("Handler error: ") + e.what());
        } catch (...) {
            serverData.recordError();
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
            _queueSize.fetch_sub(1, std::memory_order_relaxed);
            if (_maxQueueSize > 0) {
                const float fill = 100.0f * static_cast<float>(_queueSize.load(std::memory_order_relaxed)) /
                                   static_cast<float>(_maxQueueSize);
                serverData.recordQueueFill(fill > 100.0f ? 100.0f : (fill < 0.0f ? 0.0f : fill));
            }
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

void Geruest::enableStatus(const std::string& token) {
    _statusToken = token;
    _statusActive = true;

    serverData.addRoute("/status", [this, token](const HTTPRequest& req) -> HTTPResponse {
        // Verify Bearer token (skipped in dev mode)
        if (!serverData.isDevMode()) {
            const std::string authHeader = req.getHeader("authorization");
            const std::string expected   = "Bearer " + token;
            if (authHeader != expected) {
                HTTPResponse resp("401 Unauthorized");
                resp.setHeader("WWW-Authenticate", "Bearer realm=\"status\"");
                resp.setHeader("Content-Type", "application/json");
                resp.setBody(R"({"error":"Unauthorized"})");
                return resp;
            }
        }

        // Collect metrics
        const uint64_t uptime   = serverData.getUptimeSeconds();
        const ServerData::WindowMetrics wmHour = serverData.getWindowMetricsHour();
        const ServerData::WindowMetrics avgHour = serverData.getRollingAveragePerHour();
        const uint64_t totalR   = serverData.getTotalRequests();
        const uint64_t totalE   = serverData.getTotalErrors();
        const uint64_t total4xx = serverData.getTotal4xx();
        const uint64_t total5xx = serverData.getTotal5xx();
        const uint64_t totalInt = serverData.getTotalInternalErrors();
        const uint64_t rejTotal = serverData.getQueueRejections();
        const int64_t  active   = serverData.getActiveHandlers();
        const ServerData::LatencyStats lat = serverData.getLatencyStats(60);
        const uint64_t curQueue = static_cast<uint64_t>(_queueSize.load(std::memory_order_relaxed));

        std::string health = "ok";
        if (wmHour.avg_queue_fill >= 80.0 || wmHour.requests >= 1000) {
            health = "overloaded";
        } else if (wmHour.avg_queue_fill >= 50.0 || wmHour.requests >= 500) {
            health = "degraded";
        }

        // Build ISO 8601 UTC timestamp
        std::time_t now_t = std::time(nullptr);
        char timeBuf[32] = {};
#ifdef _WIN32
        struct tm utcTm{};
        gmtime_s(&utcTm, &now_t);
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
#else
        struct tm utcTm{};
        gmtime_r(&now_t, &utcTm);
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
#endif

        JSONParser requests;
        requests.setLongLong("total",    static_cast<long long>(totalR));
        requests.setLongLong("active",   static_cast<long long>(active));
        requests.setLongLong("last_hour", static_cast<long long>(wmHour.requests));
        requests.setLongLong("avg_per_hour", static_cast<long long>(avgHour.requests));

        JSONParser errors;
        errors.setLongLong("total",          static_cast<long long>(totalE));
        errors.setLongLong("client_4xx",     static_cast<long long>(total4xx));
        errors.setLongLong("server_5xx",     static_cast<long long>(total5xx));
        errors.setLongLong("internal",       static_cast<long long>(totalInt));
        errors.setLongLong("last_hour_4xx",   static_cast<long long>(wmHour.errors_4xx));
        errors.setLongLong("last_hour_5xx",   static_cast<long long>(wmHour.errors_5xx));
        errors.setLongLong("last_hour_int",   static_cast<long long>(wmHour.errors_int));
        errors.setLongLong("avg_per_hour_4xx", static_cast<long long>(avgHour.errors_4xx));
        errors.setLongLong("avg_per_hour_5xx", static_cast<long long>(avgHour.errors_5xx));
        errors.setLongLong("avg_per_hour_int", static_cast<long long>(avgHour.errors_int));

        JSONParser queue;
        queue.setLongLong("current_size",      static_cast<long long>(curQueue));
        queue.setLongLong("max_size",          static_cast<long long>(_maxQueueSize));
        queue.setLongLong("rejections_total",  static_cast<long long>(rejTotal));
        queue.setDouble("avg_fill_percent_hour", wmHour.avg_queue_fill);
        queue.setDouble("avg_fill_percent_per_hour", avgHour.avg_queue_fill);

        JSONParser latency;
        latency.setDouble("p50", lat.p50);
        latency.setDouble("p95", lat.p95);
        latency.setDouble("p99", lat.p99);

        JSONParser root;
        root.setString("health",          health);
        root.setString("version",         getVersion());
        root.setString("timestamp",       timeBuf);
        root.setLongLong("uptime_seconds", static_cast<long long>(uptime));
        root.setJSON("requests",          requests);
        root.setJSON("errors",            errors);
        root.setJSON("queue",             queue);
        root.setJSON("latency_ms",        latency);

        HTTPResponse resp("200 OK");
        resp.setHeader("Content-Type", "application/json");
        resp.setHeader("Cache-Control", "no-store");
        resp.setBody(root.toString());
        return resp;
    });

    sendToLogger("Status endpoint activated at /status (token-protected)");
}  // namespace geruest

}
