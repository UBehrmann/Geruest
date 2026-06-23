#include "email/EmailConfig.hpp"

#include "Geruest.hpp"
#include "config/ConfigLoader.hpp"
#include "email/EmailSender.hpp"

namespace geruest::email {

void applyFromConfigLoader(Geruest& server) {
#if GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
    if (!server._configFlags.emailInitialized) {
        std::string smtpServer      = ConfigLoader::get("SMTP_SERVER", "");
        std::string smtpUsername    = ConfigLoader::get("SMTP_USERNAME", "");
        std::string smtpPassword    = ConfigLoader::get("SMTP_PASSWORD", "");
        std::string smtpFromAddress = ConfigLoader::get("SMTP_FROM_ADDRESS", "");

        if (!smtpServer.empty() && !smtpUsername.empty() && !smtpPassword.empty()) {
            int  smtpPort   = ConfigLoader::getInt("SMTP_PORT", 587);
            bool smtpUseTLS = ConfigLoader::getBool("SMTP_USE_TLS", true);

            if (smtpFromAddress.empty()) {
                smtpFromAddress = smtpUsername;
            }

            server.initEmail(smtpServer, smtpPort, smtpUsername, smtpPassword, smtpFromAddress, smtpUseTLS);
            server.sendToLogger("Email sender initialized from config: " + smtpServer + ":" +
                                std::to_string(smtpPort));
        }
    }

    try {
        auto& emailSender = EmailSender::getInstance();

        if (!server._configFlags.emailMinIntervalSet) {
            int v = ConfigLoader::getInt("EMAIL_MIN_INTERVAL", 60);
            if (v > 0) {
                emailSender.setMinEmailInterval(v);
                server.sendToLogger("EMAIL_MIN_INTERVAL: " + std::to_string(v) + "s");
            }
        }
        if (!server._configFlags.emailMaxPerIPSet) {
            size_t v = ConfigLoader::getSizeT("EMAIL_MAX_PER_IP", 10);
            if (v > 0) {
                emailSender.setMaxEmailsPerIP(v);
                server.sendToLogger("EMAIL_MAX_PER_IP: " + std::to_string(v));
            }
        }
        if (!server._configFlags.emailTrackingDurationSet) {
            int v = ConfigLoader::getInt("EMAIL_TRACKING_DURATION", 3600);
            if (v > 0) {
                emailSender.setIPTrackingDuration(v);
                server.sendToLogger("EMAIL_TRACKING_DURATION: " + std::to_string(v) + "s");
            }
        }
        if (!server._configFlags.emailMaxQueueSizeSet) {
            size_t v = ConfigLoader::getSizeT("EMAIL_MAX_QUEUE_SIZE", 1000);
            if (v > 0) {
                emailSender.setMaxQueueSize(v);
                server.sendToLogger("EMAIL_MAX_QUEUE_SIZE: " + std::to_string(v));
            }
        }
    } catch (const std::runtime_error&) {
        // Email sender not initialized — optional
    }
#else
    (void)server;
#endif
}

}  // namespace geruest::email
