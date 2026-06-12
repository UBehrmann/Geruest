/**
 * @file server/WebSocketTypes.hpp
 * @brief Shared WebSocket types (limits, opcodes) for ServerData and WebSocketConnection.
 */

#ifndef GERUEST_WEBSOCKETTYPES_HPP
#define GERUEST_WEBSOCKETTYPES_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace geruest {

enum class WSOpcode : uint8_t {
    Continuation = 0x0,
    Text         = 0x1,
    Binary       = 0x2,
    Close        = 0x8,
    Ping         = 0x9,
    Pong         = 0xA,
};

struct WebSocketLimits {
    size_t               maxFrameBytes   = 4 * 1024 * 1024;
    size_t               maxMessageBytes = 16 * 1024 * 1024;
    std::chrono::seconds idleTimeout{0};
    std::chrono::seconds pingInterval{0};
};

}  // namespace geruest

#endif  // GERUEST_WEBSOCKETTYPES_HPP
