#include "Geruest.hpp"

#include "email/EmailConfig.hpp"
#include "email/EmailSender.hpp"
#include "geruest/BuildConfig.hpp"
#include "modules/ModuleHooks.hpp"

#if GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL

namespace geruest::email {

int& emailModuleLinkAnchor() {
    static int anchor = 0;
    return anchor;
}

}  // namespace geruest::email

namespace geruest {

void Geruest::initEmail(const std::string& smtpServer, int smtpPort, const std::string& username,
                        const std::string& password, const std::string& fromAddress, bool useTLS) {
    EmailSender::Config config;
    config.smtpServer  = smtpServer;
    config.port        = smtpPort;
    config.username    = username;
    config.password    = password;
    config.fromAddress = fromAddress;
    config.useTLS      = useTLS;
    EmailSender::init(config);
    _configFlags.emailInitialized = true;
    sendToLogger("Email sender initialized: " + smtpServer + ":" + std::to_string(smtpPort));
}

void Geruest::setEmailMinInterval(int seconds) {
    try {
        EmailSender::getInstance().setMinEmailInterval(seconds);
        _configFlags.emailMinIntervalSet = true;
        sendToLogger("Email min interval set to: " + std::to_string(seconds) + "s");
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set email interval - email sender not initialized");
    }
}

void Geruest::setEmailMaxPerIP(size_t count) {
    try {
        EmailSender::getInstance().setMaxEmailsPerIP(count);
        _configFlags.emailMaxPerIPSet = true;
        sendToLogger("Email max per IP set to: " + std::to_string(count));
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set max emails per IP - email sender not initialized");
    }
}

void Geruest::setEmailTrackingDuration(int seconds) {
    try {
        EmailSender::getInstance().setIPTrackingDuration(seconds);
        _configFlags.emailTrackingDurationSet = true;
        sendToLogger("Email tracking duration set to: " + std::to_string(seconds) + "s");
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set tracking duration - email sender not initialized");
    }
}

void Geruest::setEmailMaxQueueSize(size_t size) {
    try {
        EmailSender::getInstance().setMaxQueueSize(size);
        _configFlags.emailMaxQueueSizeSet = true;
        sendToLogger("Email max queue size set to: " + std::to_string(size));
    } catch (const std::runtime_error&) {
        sendToLoggerError("Cannot set email queue size - email sender not initialized");
    }
}

}  // namespace geruest

#endif  // GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
