/**
 * @file Database.hpp
 * @brief Database module — link with Geruest::Database.
 */
#ifndef GERUEST_DATABASE_MODULE_HPP
#define GERUEST_DATABASE_MODULE_HPP

#include "geruest/BuildConfig.hpp"
#if GERUEST_HAS_LIBPQ || GERUEST_HAS_SQLITE
#include "database/DatabaseClient.hpp"
#endif

#endif
