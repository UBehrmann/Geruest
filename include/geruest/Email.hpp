/**
 * @file Email.hpp
 * @brief Email module — link with Geruest::Email.
 */
#ifndef GERUEST_EMAIL_MODULE_HPP
#define GERUEST_EMAIL_MODULE_HPP

#include "geruest/BuildConfig.hpp"
#if GERUEST_HAS_CURL && GERUEST_ENABLE_EMAIL
#include "email/EmailSender.hpp"
#include "email/EmailConfig.hpp"
#endif

#endif
