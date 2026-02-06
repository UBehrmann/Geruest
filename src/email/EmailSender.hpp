/**
 * @file EmailSender.hpp
 * @date 05.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Email sending system with producer-consumer pattern and spam protection
 */

#ifndef GERUEST_EMAILSENDER_HPP
#define GERUEST_EMAILSENDER_HPP

#if GERUEST_HAS_CURL

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define EMAIL_WORKER_COUNT 3
#define EMAIL_WORKER_IDLE_TIMEOUT_SECONDS 10

namespace geruest {

/**
 * @brief Structure representing an email request
 */
struct Email {
    std::string to;
    std::string subject;
    std::string body;
    std::string clientIP;
    std::chrono::steady_clock::time_point timestamp;
    int retries = 0;

    Email() : timestamp(std::chrono::steady_clock::now()) {}
    
    Email(const std::string& to_, const std::string& subject_,
          const std::string& body_, const std::string& clientIP_)
        : to(to_),
          subject(subject_),
          body(body_),
          clientIP(clientIP_),
          timestamp(std::chrono::steady_clock::now()),
          retries(0) {}
};

/**
 * @brief Structure to track IP activity for spam prevention
 */
struct IPActivity {
    std::chrono::steady_clock::time_point lastEmailTime;
    size_t emailCount;
    
    IPActivity() 
        : lastEmailTime(std::chrono::steady_clock::now()), 
          emailCount(0) {}
};

/**
 * @brief Email sender with producer-consumer pattern and spam protection
 * 
 * Features:
 * - Thread-safe producer-consumer queue
 * - Dynamic worker thread management
 * - IP-based rate limiting to prevent spam
 * - Automatic cleanup of old IP records
 * - CURL-based SMTP support
 * - Automatic retry logic for failed sends
 */
class EmailSender {
   public:
    /**
     * @brief SMTP configuration structure
     */
    struct Config {
        std::string smtpServer;
        int port = 587;
        std::string username;
        std::string password;
        bool useTLS = true;
        std::string fromAddress;
    };

    /**
     * @brief Initialize the EmailSender singleton
     * @param config SMTP configuration
     * @note Must be called once before getInstance()
     */
    static void init(const Config& config);

    /**
     * @brief Get the EmailSender singleton instance
     * @return Reference to the singleton instance
     * @throws std::runtime_error if not initialized
     */
    static EmailSender& getInstance();

    /**
     * @brief Stop the email sender and cleanup
     */
    void stop();

    /**
     * @brief Queue an email for sending (producer)
     * @param to Recipient email address
     * @param subject Email subject
     * @param body Email body (plain text or HTML)
     * @param clientIP IP address of the client requesting the email
     * @return true if email was queued, false if rejected due to spam protection
     */
    bool enqueueEmail(const std::string& to, const std::string& subject,
                      const std::string& body, const std::string& clientIP);

    /**
     * @brief Set the minimum time interval between emails from the same IP
     * @param seconds Minimum seconds between emails (default: 60)
     */
    void setMinEmailInterval(int seconds);

    /**
     * @brief Set the maximum number of emails per IP in the tracking window
     * @param count Maximum emails allowed (default: 10)
     */
    void setMaxEmailsPerIP(size_t count);

    /**
     * @brief Set how long to remember IP activity
     * @param seconds Seconds to track IP (default: 3600 = 1 hour)
     */
    void setIPTrackingDuration(int seconds);

    /**
     * @brief Set the maximum queue size
     * @param size Maximum pending emails (default: 1000)
     */
    void setMaxQueueSize(size_t size);

    /**
     * @brief Get the number of emails currently in the queue
     * @return Number of pending emails
     */
    size_t getQueueSize() const;

    /**
     * @brief Get the number of emails sent since start
     * @return Total emails processed
     */
    size_t getEmailsSent() const;

    /**
     * @brief Get the number of emails rejected due to spam protection
     * @return Total emails rejected
     */
    size_t getEmailsRejected() const;

    /**
     * @brief Clear all IP tracking data
     */
    void clearIPTracking();

   private:
    EmailSender(const Config& config);
    ~EmailSender();

    // Singleton
    static EmailSender* instance;
    static std::mutex instanceMutex;

    // Configuration
    Config config;
    size_t _maxQueueSize = 1000;

    // Spam protection configuration
    int _minEmailIntervalSeconds = 60;
    size_t _maxEmailsPerIP = 10;
    int _ipTrackingDurationSeconds = 3600;

    // Thread pool components
    std::unordered_set<std::thread::id> activeWorkers;
    std::vector<std::thread> threads;
    std::queue<Email> emailQueue;
    std::mutex queueMutex;
    std::condition_variable condVar;
    std::atomic<bool> shuttingDown{false};

    // Spam protection
    std::unordered_map<std::string, IPActivity> _ipTracking;
    mutable std::mutex _ipTrackingMutex;

    // Statistics
    std::atomic<size_t> _emailsSent{0};
    std::atomic<size_t> _emailsRejected{0};

    /**
     * @brief Worker thread function (consumer)
     */
    void worker();

    /**
     * @brief Atomically check spam limits and update IP tracking if allowed
     * @param clientIP The client's IP address
     * @return true if allowed and tracking updated, false if blocked
     * @note This combines check + update in one critical section to prevent race conditions
     */
    bool checkAndUpdateSpamProtection(const std::string& clientIP);

    /**
     * @brief Clean up old IP tracking records
     */
    void cleanupOldIPRecords();

    /**
     * @brief Send email via SMTP using CURL
     * @param email The email to send
     * @return true if sent successfully
     */
    bool sendEmail(const Email& email);

    /**
     * @brief Log message to server logger
     * @param message The message to log
     */
    void sendToLogger(const std::string& message) const;

    /**
     * @brief Log error message to server logger
     * @param message The error message to log
     */
    void sendToLoggerError(const std::string& message) const;
};

}  // namespace geruest

#endif  // GERUEST_HAS_CURL
#endif  // GERUEST_EMAILSENDER_HPP
