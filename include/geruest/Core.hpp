/**
 * @file Core.hpp
 * @brief Geruest HTTP core — link with Geruest::Core.
 *
 * Includes the full Geruest server class plus HTTP/JSON/config types.
 * For optional modules (WebSocket, Assets, Obfuscation), use <geruest/Geruest.hpp>
 * or include <geruest/WebSocket.hpp> etc. and link the matching CMake target.
 */
#ifndef GERUEST_CORE_HPP
#define GERUEST_CORE_HPP

#include "geruest/BuildConfig.hpp"
#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerTypes.hpp"
#include "data/CorsConfig.hpp"
#include "parser/JSONParser.hpp"
#include "security/Security.hpp"
#include "config/ConfigLoader.hpp"
#include "Geruest.hpp"

#endif
