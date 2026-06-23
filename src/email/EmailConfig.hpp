/**
 * @file EmailConfig.hpp
 * @brief SMTP configuration from ConfigLoader (Geruest::Email module).
 */

#ifndef GERUEST_EMAIL_EMAILCONFIG_HPP
#define GERUEST_EMAIL_EMAILCONFIG_HPP

namespace geruest {

class Geruest;

namespace email {

/** Apply SMTP settings from ConfigLoader; safe when EmailSender is not initialized. */
void applyFromConfigLoader(Geruest& server);

int& emailModuleLinkAnchor();

}  // namespace email
}  // namespace geruest

#endif
