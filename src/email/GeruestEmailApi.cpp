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
    _emailSender = std::make_unique<EmailSender>(config);
    _emailSender->setLogCallbacks(
        [this](std::string_view msg) { sendToLogger(std::string(msg)); },
        [this](std::string_view msg) { sendToLoggerError(std::string(msg)); });
    _configFlags.emailInitialized = true;
    sendToLogger("Email sender initialized: " + smtpServer + ":" + std::to_string(smtpPort));
}

EmailSender* Geruest::emailSender() { return _emailSender.get(); }

const EmailSender* Geruest::emailSender() const { return _emailSender.get(); }

void Geruest::setEmailMinInterval(int seconds) {
    if (!_emailSender) {
        sendToLoggerError("Cannot set email interval - email sender not initialized");
        return;
    }
    _emailSender->setMinEmailInterval(seconds);
    _configFlags.emailMinIntervalSet = true;
    sendToLogger("Email min interval set to: " + std::to_string(seconds) + "s");
}

void Geruest::setEmailMaxPerIP(size_t count) {
    if (!_emailSender) {
        sendToLoggerError("Cannot set max emails per IP - email sender not initialized");
        return;
    }
    _emailSender->setMaxEmailsPerIP(count);
    _configFlags.emailMaxPerIPSet = true;
    sendToLogger("Email max per IP set to: " + std::to_string(count));
}

void Geruest::setEmailTrackingDuration(int seconds) {
    if (!_emailSender) {
        sendToLoggerError("Cannot set tracking duration - email sender not initialized");
        return;
    }
    _emailSender->setIPTrackingDuration(seconds);
    _configFlags.emailTrackingDurationSet = true;
    sendToLogger("Email tracking duration set to: " + std::to_string(seconds) + "s");
}

void Geruest::setEmailMaxQueueSize(size_t size) {
    if (!_emailSender) {
        sendToLoggerError("Cannot set email queue size - email sender not initialized");
        return;
    }
    _emailSender->setMaxQueueSize(size);
    _configFlags.emailMaxQueueSizeSet = true;
    sendToLogger("Email max queue size set to: " + std::to_string(size));
}

}  // namespace geruest

#endif  // GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
