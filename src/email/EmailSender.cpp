/**
 * @file EmailSender.cpp
 * @date 05.02.2026
 *
 * @author Urs Behrmann
 *
 * @brief Implementation of email sending system with spam protection
 */

#include "EmailSender.hpp"

#if GERUEST_HAS_CURL

#include <algorithm>
#include <cstring>
#include <iostream>

#include <curl/curl.h>

namespace geruest {

EmailSender* EmailSender::instance = nullptr;
std::mutex EmailSender::instanceMutex;

void EmailSender::init(const Config& config) {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        instance = new EmailSender(config);
    }
}

EmailSender& EmailSender::getInstance() {
    if (!instance) {
        throw std::runtime_error("EmailSender not initialized. Call init() first.");
    }
    return *instance;
}

EmailSender::EmailSender(const Config& cfg)
    : config(cfg), shuttingDown(false) {}

EmailSender::~EmailSender() {
    stop();
    curl_global_cleanup();
}

void EmailSender::stop() {
    shuttingDown = true;
    condVar.notify_all();

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    threads.clear();
    activeWorkers.clear();
}

bool EmailSender::enqueueEmail(const std::string& to, const std::string& subject,
                               const std::string& body, const std::string& clientIP) {
    // Check spam protection
    if (!checkSpamProtection(clientIP)) {
        _emailsRejected++;
        return false;
    }

    // Check queue size and add email
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (emailQueue.size() >= _maxQueueSize) {
            _emailsRejected++;
            sendToLoggerError("EmailSender: Queue full, rejecting email from " + clientIP);
            return false;
        }

        emailQueue.emplace(to, subject, body, clientIP);

        // Create new worker thread if needed
        if (activeWorkers.size() < EMAIL_WORKER_COUNT) {
            threads.emplace_back(&EmailSender::worker, this);
        }
    }

    // Update IP tracking
    updateIPActivity(clientIP);

    // Notify one worker
    condVar.notify_one();
    return true;
}

void EmailSender::setMinEmailInterval(int seconds) {
    _minEmailIntervalSeconds = seconds;
}

void EmailSender::setMaxEmailsPerIP(size_t count) {
    _maxEmailsPerIP = count;
}

void EmailSender::setIPTrackingDuration(int seconds) {
    _ipTrackingDurationSeconds = seconds;
}

void EmailSender::setMaxQueueSize(size_t size) {
    _maxQueueSize = size;
}

size_t EmailSender::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return emailQueue.size();
}

size_t EmailSender::getEmailsSent() const {
    return _emailsSent.load();
}

size_t EmailSender::getEmailsRejected() const {
    return _emailsRejected.load();
}

void EmailSender::clearIPTracking() {
    std::lock_guard<std::mutex> lock(_ipTrackingMutex);
    _ipTracking.clear();
}

void EmailSender::worker() {
    std::thread::id id = std::this_thread::get_id();
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        activeWorkers.insert(id);
    }

    while (true) {
        Email email;
        bool hasEmail = false;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condVar.wait_for(lock, std::chrono::seconds(EMAIL_WORKER_IDLE_TIMEOUT_SECONDS), [this]() {
                return !emailQueue.empty() || shuttingDown;
            });

            if (emailQueue.empty()) {
                if (shuttingDown) break;
                break;  // Idle timeout
            }

            email = emailQueue.front();
            emailQueue.pop();
            hasEmail = true;
        }

        if (hasEmail) {
            if (sendEmail(email)) {
                _emailsSent++;
                sendToLogger("Email sent from " + config.fromAddress + " to " + email.to);
            } else {
                // Retry logic
                if (email.retries < 3) {
                    Email retry = email;
                    retry.retries++;
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    
                    std::lock_guard<std::mutex> lock(queueMutex);
                    emailQueue.push(retry);
                    condVar.notify_one();
                } else {
                    sendToLoggerError("EmailSender: Failed to send email to " + email.to + 
                                    " after 3 retries");
                }
            }

            // Periodically cleanup old IP records (every 100 emails)
            if (_emailsSent.load() % 100 == 0) {
                cleanupOldIPRecords();
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        activeWorkers.erase(id);
    }

}

bool EmailSender::checkSpamProtection(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(_ipTrackingMutex);

    auto now = std::chrono::steady_clock::now();
    auto it = _ipTracking.find(clientIP);

    if (it == _ipTracking.end()) {
        // New IP, allow
        return true;
    }

    auto& activity = it->second;
    auto timeSinceLastEmail = std::chrono::duration_cast<std::chrono::seconds>(
        now - activity.lastEmailTime).count();

    // Check minimum interval
    if (timeSinceLastEmail < _minEmailIntervalSeconds) {
        return false;
    }

    // Check maximum emails per tracking window
    if (activity.emailCount >= _maxEmailsPerIP && 
        timeSinceLastEmail < _ipTrackingDurationSeconds) {
        return false;
    }

    return true;
}

void EmailSender::updateIPActivity(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(_ipTrackingMutex);

    auto now = std::chrono::steady_clock::now();
    auto it = _ipTracking.find(clientIP);

    if (it == _ipTracking.end()) {
        // New IP
        IPActivity activity;
        activity.lastEmailTime = now;
        activity.emailCount = 1;
        _ipTracking[clientIP] = activity;
    } else {
        // Existing IP
        auto& activity = it->second;
        auto timeSinceLastEmail = std::chrono::duration_cast<std::chrono::seconds>(
            now - activity.lastEmailTime).count();

        // Reset count if tracking window has passed
        if (timeSinceLastEmail >= _ipTrackingDurationSeconds) {
            activity.emailCount = 1;
        } else {
            activity.emailCount++;
        }
        
        activity.lastEmailTime = now;
    }
}

void EmailSender::cleanupOldIPRecords() {
    std::lock_guard<std::mutex> lock(_ipTrackingMutex);

    auto now = std::chrono::steady_clock::now();
    size_t removedCount = 0;

    for (auto it = _ipTracking.begin(); it != _ipTracking.end();) {
        auto timeSinceLastEmail = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastEmailTime).count();

        // Remove records older than 2x tracking duration
        if (timeSinceLastEmail > _ipTrackingDurationSeconds * 2) {
            it = _ipTracking.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
}

static size_t payloadSource(char* ptr, size_t size, size_t nmemb, void* userp) {
    auto* p = static_cast<std::pair<const char*, size_t>*>(userp);
    size_t max = size * nmemb;
    size_t toCopy = (std::min)(p->second, max);
    memcpy(ptr, p->first, toCopy);
    p->first += toCopy;
    p->second -= toCopy;
    return toCopy;
}

bool EmailSender::sendEmail(const Email& email) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string from = "From: " + config.fromAddress + "\r\n";
    std::string to = "To: " + email.to + "\r\n";
    std::string subject = "Subject: " + email.subject + "\r\n";
    std::string data = from + to + subject + "\r\n" + email.body + "\r\n";

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, email.to.c_str());

    // Port 465 uses implicit SSL (smtps://), port 587 uses explicit STARTTLS (smtp://)
    std::string url;
    if (config.port == 465) {
        url = "smtps://" + config.smtpServer + ":465";
    } else {
        url = "smtp://" + config.smtpServer + ":" + std::to_string(config.port);
    }

    // Store strings as variables to ensure they live long enough for CURL
    std::string mailFrom = "<" + config.fromAddress + ">";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mailFrom.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_USERNAME, config.username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, config.password.c_str());

    // For port 587 with STARTTLS
    if (config.port == 587) {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
    } else {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    }

    const char* payload = data.c_str();
    size_t payloadLeft = data.size();

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payloadSource);

    std::pair<const char*, size_t> payloadData = {payload, payloadLeft};
    curl_easy_setopt(curl, CURLOPT_READDATA, &payloadData);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    CURLcode res = curl_easy_perform(curl);
    
    // Log detailed error information
    if (res != CURLE_OK) {
        std::string errorMsg = "SMTP Error: ";
        errorMsg += curl_easy_strerror(res);
        errorMsg += " (Server: " + config.smtpServer + ":" + std::to_string(config.port) + ")";
        sendToLoggerError(errorMsg);
    }

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

void EmailSender::sendToLogger(const std::string& message) const {
    std::cout << "[EmailSender] " << message << std::endl;
}

void EmailSender::sendToLoggerError(const std::string& message) const {
    std::cerr << "[EmailSender ERROR] " << message << std::endl;
}

}  // namespace geruest

#endif  // GERUEST_HAS_CURL
