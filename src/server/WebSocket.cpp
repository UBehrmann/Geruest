/**
 * @file server/WebSocket.cpp
 */

#include "WebSocket.hpp"

#include "security/Base64.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace geruest {
namespace {

constexpr char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

[[nodiscard]] inline unsigned char asciiLower(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] bool iequalsAscii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (asciiLower(static_cast<unsigned char>(a[i])) !=
            asciiLower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string_view trimSv(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

[[nodiscard]] bool headerContainsToken(std::string_view value, std::string_view token) {
    value = trimSv(value);
    size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() && (value[i] == ' ' || value[i] == '\t' || value[i] == ',')) {
            ++i;
        }
        size_t j = i;
        while (j < value.size() && value[j] != ',') {
            ++j;
        }
        if (iequalsAscii(trimSv(value.substr(i, j - i)), token)) {
            return true;
        }
        i = j + 1;
    }
    return false;
}

[[nodiscard]] bool isControlOpcode(WSOpcode op) {
    const uint8_t v = static_cast<uint8_t>(op);
    return v >= 0x8 && v <= 0xF;
}

void appendUint16BE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void unmaskPayload(std::vector<uint8_t>& payload, const uint8_t mask[4]) {
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] ^= mask[i % 4];
    }
}

}  // namespace

namespace websocket_codec {

std::string sha1Hash(std::string_view input) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    std::vector<uint8_t> msg(input.begin(), input.end());
    const uint64_t       bitLen = static_cast<uint64_t>(msg.size()) * 8ULL;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80]{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(msg[chunk + i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = ((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]) << 1) |
                   ((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]) >> 31);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const uint32_t temp =
                ((a << 5) | (a >> 27)) + f + e + k + w[static_cast<size_t>(i)];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::string digest(20, '\0');
    const uint32_t hs[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        digest[static_cast<size_t>(i * 4)]     = static_cast<char>((hs[i] >> 24) & 0xFF);
        digest[static_cast<size_t>(i * 4 + 1)] = static_cast<char>((hs[i] >> 16) & 0xFF);
        digest[static_cast<size_t>(i * 4 + 2)] = static_cast<char>((hs[i] >> 8) & 0xFF);
        digest[static_cast<size_t>(i * 4 + 3)] = static_cast<char>(hs[i] & 0xFF);
    }
    return digest;
}

std::vector<uint8_t> encodeServerFrame(WSOpcode opcode, std::span<const uint8_t> payload, bool fin) {
    const size_t len = payload.size();
    size_t         headerSize = 2;
    if (len >= 126 && len <= 0xFFFF) {
        headerSize = 4;
    } else if (len > 0xFFFF) {
        headerSize = 10;
    }

    std::vector<uint8_t> frame(headerSize + len);
    size_t               pos = 0;

    uint8_t b0 = static_cast<uint8_t>(opcode) & 0x0F;
    if (fin) {
        b0 |= 0x80;
    }
    frame[pos++] = b0;

    if (len < 126) {
        frame[pos++] = static_cast<uint8_t>(len);
    } else if (len <= 0xFFFF) {
        const uint16_t len16 = static_cast<uint16_t>(len);
        frame[pos++] = 126;
        frame[pos++] = static_cast<uint8_t>((len16 >> 8) & 0xFF);
        frame[pos++] = static_cast<uint8_t>(len16 & 0xFF);
    } else {
        frame[pos++] = 127;
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame[pos++] = static_cast<uint8_t>((len >> shift) & 0xFF);
        }
    }

    if (len > 0) {
        std::memcpy(frame.data() + pos, payload.data(), len);
    }
    return frame;
}

WSMessage decodeClientFrame(std::span<const uint8_t> bytes, const WebSocketLimits& limits,
                            std::span<const uint8_t>* remainder) {
    if (remainder) {
        *remainder = {};
    }
    if (bytes.size() < 2) {
        throw std::runtime_error("incomplete websocket frame");
    }

    const bool fin  = (bytes[0] & 0x80) != 0;
    const auto op   = static_cast<WSOpcode>(bytes[0] & 0x0F);
    const bool mask = (bytes[1] & 0x80) != 0;
    uint64_t     payloadLen = bytes[1] & 0x7F;
    size_t       headerLen  = 2;

    if (!mask) {
        throw std::runtime_error("client frame must be masked");
    }

    if (payloadLen == 126) {
        if (bytes.size() < 4) {
            throw std::runtime_error("incomplete extended length");
        }
        payloadLen = (static_cast<uint64_t>(bytes[2]) << 8) | bytes[3];
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (bytes.size() < 10) {
            throw std::runtime_error("incomplete extended length");
        }
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | bytes[2 + static_cast<size_t>(i)];
        }
        headerLen = 10;
    }

    if (payloadLen > limits.maxFrameBytes) {
        throw std::runtime_error("frame too large");
    }

    const size_t total = headerLen + 4 + static_cast<size_t>(payloadLen);
    if (bytes.size() < total) {
        throw std::runtime_error("incomplete websocket frame");
    }

    uint8_t maskKey[4];
    std::memcpy(maskKey, bytes.data() + headerLen, 4);
    std::vector<uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(headerLen + 4),
                                 bytes.begin() + static_cast<std::ptrdiff_t>(total));
    unmaskPayload(payload, maskKey);

    if (remainder) {
        *remainder = bytes.subspan(total);
    }

    if (isControlOpcode(op) && !fin) {
        throw std::runtime_error("fragmented control frame");
    }
    if (isControlOpcode(op) && payload.size() > 125) {
        throw std::runtime_error("control frame too large");
    }

    if (op == WSOpcode::Close) {
        uint16_t code = 1005;
        std::string reason;
        if (payload.size() >= 2) {
            code = static_cast<uint16_t>((payload[0] << 8) | payload[1]);
            if (payload.size() > 2) {
                reason.assign(payload.begin() + 2, payload.end());
            }
        }
        return WSMessage::makeClose(code, reason);
    }

    if (op == WSOpcode::Text) {
        return WSMessage::makeText(
            std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    }
    if (op == WSOpcode::Binary) {
        return WSMessage::makeBinary(payload);
    }
    if (op == WSOpcode::Ping) {
        return WSMessage::makePing(std::move(payload));
    }
    if (op == WSOpcode::Pong) {
        return WSMessage::makePong(std::move(payload));
    }
    (void)fin;
    return WSMessage::makeBinary(payload);
}

}  // namespace websocket_codec

WSMessage WSMessage::makeText(std::string_view text) {
    WSMessage m;
    m._opcode = WSOpcode::Text;
    m._payload.assign(reinterpret_cast<const uint8_t*>(text.data()),
                      reinterpret_cast<const uint8_t*>(text.data() + text.size()));
    return m;
}

WSMessage WSMessage::makeBinary(std::span<const uint8_t> data) {
    WSMessage m;
    m._opcode = WSOpcode::Binary;
    m._payload.assign(data.begin(), data.end());
    return m;
}

WSMessage WSMessage::makeClose(uint16_t code, std::string_view reason) {
    WSMessage m;
    m._opcode = WSOpcode::Close;
    m._closeCode = code;
    m._closeReason.assign(reason);
    m._payload.reserve(2 + reason.size());
    appendUint16BE(m._payload, code);
    m._payload.insert(m._payload.end(), reason.begin(), reason.end());
    return m;
}

WSMessage WSMessage::makePing(std::vector<uint8_t> payload) {
    WSMessage m;
    m._opcode = WSOpcode::Ping;
    m._payload = std::move(payload);
    return m;
}

WSMessage WSMessage::makePong(std::vector<uint8_t> payload) {
    WSMessage m;
    m._opcode = WSOpcode::Pong;
    m._payload = std::move(payload);
    return m;
}

bool isWebSocketUpgradeIntent(const HTTPRequest& req) {
    return req.getMethod() == "GET" && headerContainsToken(req.getHeaderView("upgrade"), "websocket");
}

bool isWebSocketUpgrade(const HTTPRequest& req) {
    if (req.getMethod() != "GET") {
        return false;
    }
    if (!headerContainsToken(req.getHeaderView("upgrade"), "websocket")) {
        return false;
    }
    if (!headerContainsToken(req.getHeaderView("connection"), "upgrade")) {
        return false;
    }
    if (!iequalsAscii(trimSv(req.getHeaderView("sec-websocket-version")), "13")) {
        return false;
    }
    if (trimSv(req.getHeaderView("sec-websocket-key")).empty()) {
        return false;
    }
    return true;
}

std::string computeAcceptKey(std::string_view secKey) {
    std::string combined;
    combined.reserve(secKey.size() + sizeof(kWebSocketGuid) - 1);
    combined.append(secKey);
    combined.append(kWebSocketGuid);
    return base64Encode(websocket_codec::sha1Hash(combined));
}

std::string buildHandshakeResponse(std::string_view acceptKey, std::string_view subprotocol) {
    std::string out = "HTTP/1.1 101 Switching Protocols\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Accept: ";
    out.append(acceptKey);
    out.append("\r\n");
    if (!subprotocol.empty()) {
        out.append("Sec-WebSocket-Protocol: ");
        out.append(subprotocol);
        out.append("\r\n");
    }
    out.append("\r\n");
    return out;
}

std::string pickSubprotocol(std::string_view clientHeader, const std::vector<std::string>& serverOffered) {
    if (clientHeader.empty() || serverOffered.empty()) {
        return {};
    }
    size_t i = 0;
    while (i < clientHeader.size()) {
        while (i < clientHeader.size() &&
               (clientHeader[i] == ' ' || clientHeader[i] == '\t' || clientHeader[i] == ',')) {
            ++i;
        }
        size_t j = i;
        while (j < clientHeader.size() && clientHeader[j] != ',') {
            ++j;
        }
        const std::string_view token = trimSv(clientHeader.substr(i, j - i));
        for (const auto& offered : serverOffered) {
            if (iequalsAscii(token, offered)) {
                return offered;
            }
        }
        i = j + 1;
    }
    return {};
}

WebSocketConnection::WriteChain::WriteChain(tcp_socket& sock) : socket(sock) {}

void WebSocketConnection::WriteChain::enqueueOnExecutor(PendingWrite item) {
    queue.push_back(std::move(item));
    startPumpIfNeeded();
}

void WebSocketConnection::WriteChain::startPumpIfNeeded() {
    if (pumping || queue.empty()) {
        return;
    }
    pumping = true;
    const std::shared_ptr<WriteChain> self = shared_from_this();
    boost::asio::co_spawn(
        socket.get_executor(),
        [self]() -> boost::asio::awaitable<void> { co_await self->pump(); },
        boost::asio::detached);
}

boost::asio::awaitable<void> WebSocketConnection::WriteChain::pump() {
    while (true) {
        PendingWrite item;
        if (queue.empty()) {
            pumping = false;
            if (queue.empty()) {
                co_return;
            }
            pumping = true;
            continue;
        }
        item = std::move(queue.front());
        queue.pop_front();

        if (!writesEnabled.load(std::memory_order_relaxed)) {
            if (item.onComplete) {
                item.onComplete();
            }
            pumping = false;
            co_return;
        }

        try {
            co_await boost::asio::async_write(socket, boost::asio::buffer(item.frame),
                                              boost::asio::use_awaitable);
        } catch (...) {
            writesEnabled.store(false, std::memory_order_relaxed);
            if (item.onComplete) {
                item.onComplete();
            }
            pumping = false;
            co_return;
        }

        if (item.onComplete) {
            item.onComplete();
        }
    }
}

void WebSocketConnection::WriteChain::schedule(std::vector<uint8_t> frame) {
    if (!writesEnabled.load(std::memory_order_relaxed)) {
        return;
    }
    const std::shared_ptr<WriteChain> self = shared_from_this();
    boost::asio::post(socket.get_executor(), [self, f = std::move(frame)]() mutable {
        if (!self->writesEnabled.load(std::memory_order_relaxed)) {
            return;
        }
        self->enqueueOnExecutor(PendingWrite{std::move(f), nullptr});
    });
}

boost::asio::awaitable<void> WebSocketConnection::WriteChain::write(std::vector<uint8_t> frame) {
    if (!writesEnabled.load(std::memory_order_relaxed)) {
        co_return;
    }
    struct Gate {
        bool done = false;
    };
    const std::shared_ptr<Gate>          gate = std::make_shared<Gate>();
    const std::shared_ptr<WriteChain>    self = shared_from_this();
    boost::asio::post(socket.get_executor(), [self, gate, f = std::move(frame)]() mutable {
        if (!self->writesEnabled.load(std::memory_order_relaxed)) {
            gate->done = true;
            return;
        }
        self->enqueueOnExecutor(PendingWrite{std::move(f), [gate]() { gate->done = true; }});
    });
    while (!gate->done) {
        co_await boost::asio::post(socket.get_executor(), boost::asio::use_awaitable);
    }
    co_return;
}

WebSocketConnection::WebSocketConnection(tcp_socket& sock, std::string clientIp,
                                         std::string selectedSubprotocol, WebSocketLimits limits)
    : _socket(sock)
    , _writeChain(std::make_shared<WriteChain>(sock))
    , _clientIp(std::move(clientIp))
    , _selectedSubprotocol(std::move(selectedSubprotocol))
    , _limits(std::move(limits)) {}

void WebSocketConnection::markClosed(uint16_t code, std::string_view reason) {
    _open = false;
    if (_writeChain) {
        _writeChain->writesEnabled.store(false, std::memory_order_relaxed);
    }
    _closeCode = code;
    _closeReason.assign(reason);
}

boost::asio::awaitable<void> WebSocketConnection::writeCloseFrame(uint16_t code, std::string_view reason) {
    if (!_writeChain) {
        co_return;
    }
    try {
        co_await _writeChain->write(websocket_codec::encodeServerFrame(
            WSOpcode::Close, WSMessage::makeClose(code, reason).data()));
    } catch (...) {
    }
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::protocolError(uint16_t code, std::string_view reason) {
    co_await writeCloseFrame(code, reason);
    markClosed(code, reason);
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::writeRawFrame(std::vector<uint8_t> frame) {
    if (!_open || !_writeChain) {
        co_return;
    }
    try {
        co_await _writeChain->write(std::move(frame));
    } catch (...) {
        markClosed(1011, "write failed");
        throw;
    }
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::writeFrame(WSOpcode op, std::span<const uint8_t> payload,
                                                             bool fin) {
    co_await writeRawFrame(websocket_codec::encodeServerFrame(op, payload, fin));
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::send(std::string_view text) {
    const auto* begin = reinterpret_cast<const uint8_t*>(text.data());
    co_await writeFrame(WSOpcode::Text, std::span<const uint8_t>(begin, text.size()));
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::sendBinary(std::span<const uint8_t> data) {
    co_await writeFrame(WSOpcode::Binary, data);
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::ping(std::span<const uint8_t> payload) {
    co_await writeFrame(WSOpcode::Ping, payload);
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::pong(std::span<const uint8_t> payload) {
    co_await writeFrame(WSOpcode::Pong, payload);
    co_return;
}

boost::asio::awaitable<void> WebSocketConnection::close(uint16_t code, std::string_view reason) {
    if (!_open) {
        co_return;
    }
    co_await writeCloseFrame(code, reason);
    markClosed(code, reason);
    co_return;
}

void WebSocketConnection::sendNow(std::string_view text) {
    if (!_writeChain) {
        return;
    }
    const auto* begin = reinterpret_cast<const uint8_t*>(text.data());
    _writeChain->schedule(websocket_codec::encodeServerFrame(
        WSOpcode::Text, std::span<const uint8_t>(begin, text.size())));
}

void WebSocketConnection::sendBinaryNow(std::span<const uint8_t> data) {
    if (!_writeChain) {
        return;
    }
    _writeChain->schedule(websocket_codec::encodeServerFrame(WSOpcode::Binary, data));
}

boost::asio::awaitable<WSMessage> WebSocketConnection::readMessage() {
    std::vector<uint8_t> accumulator;
    WSOpcode             messageOpcode = WSOpcode::Continuation;
    bool                 started       = false;

    while (_open) {
        uint8_t header[2];
        co_await boost::asio::async_read(_socket, boost::asio::buffer(header, 2), boost::asio::use_awaitable);

        const bool fin  = (header[0] & 0x80) != 0;
        const auto op   = static_cast<WSOpcode>(header[0] & 0x0F);
        const bool mask = (header[1] & 0x80) != 0;
        uint64_t     payloadLen = header[1] & 0x7F;

        if (!mask) {
            co_await protocolError(1002, "unmasked client frame");
            co_return WSMessage::makeClose(_closeCode, _closeReason);
        }

        if (payloadLen == 126) {
            uint8_t ext[2];
            co_await boost::asio::async_read(_socket, boost::asio::buffer(ext, 2), boost::asio::use_awaitable);
            payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
        } else if (payloadLen == 127) {
            uint8_t ext[8];
            co_await boost::asio::async_read(_socket, boost::asio::buffer(ext, 8), boost::asio::use_awaitable);
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | ext[static_cast<size_t>(i)];
            }
        }

        if (payloadLen > _limits.maxFrameBytes) {
            co_await protocolError(1009, "frame too large");
            co_return WSMessage::makeClose(_closeCode, _closeReason);
        }

        uint8_t maskKey[4];
        co_await boost::asio::async_read(_socket, boost::asio::buffer(maskKey, 4), boost::asio::use_awaitable);

        std::vector<uint8_t> payload(static_cast<size_t>(payloadLen));
        if (payloadLen > 0) {
            co_await boost::asio::async_read(_socket, boost::asio::buffer(payload), boost::asio::use_awaitable);
            unmaskPayload(payload, maskKey);
        }

        if (isControlOpcode(op) && !fin) {
            co_await protocolError(1002, "fragmented control");
            co_return WSMessage::makeClose(_closeCode, _closeReason);
        }
        if (isControlOpcode(op) && payload.size() > 125) {
            co_await protocolError(1002, "control frame too large");
            co_return WSMessage::makeClose(_closeCode, _closeReason);
        }

        if (op == WSOpcode::Ping) {
            co_await writeFrame(WSOpcode::Pong, payload);
            continue;
        }
        if (op == WSOpcode::Pong) {
            continue;
        }
        if (op == WSOpcode::Close) {
            uint16_t code = 1005;
            std::string reason;
            if (payload.size() >= 2) {
                code = static_cast<uint16_t>((payload[0] << 8) | payload[1]);
                if (payload.size() > 2) {
                    reason.assign(payload.begin() + 2, payload.end());
                }
            }
            co_await writeCloseFrame(code, reason);
            markClosed(code, reason);
            co_return WSMessage::makeClose(code, reason);
        }

        if (op != WSOpcode::Continuation) {
            messageOpcode = op;
            started = true;
            accumulator = std::move(payload);
        } else if (started) {
            if (accumulator.size() + payload.size() > _limits.maxMessageBytes) {
                co_await protocolError(1009, "message too large");
                co_return WSMessage::makeClose(_closeCode, _closeReason);
            }
            accumulator.insert(accumulator.end(), payload.begin(), payload.end());
        } else {
            co_await protocolError(1002, "unexpected continuation");
            co_return WSMessage::makeClose(_closeCode, _closeReason);
        }

        if (fin && started) {
            if (messageOpcode == WSOpcode::Text) {
                co_return WSMessage::makeText(std::string_view(
                    reinterpret_cast<const char*>(accumulator.data()), accumulator.size()));
            }
            co_return WSMessage::makeBinary(accumulator);
        }
    }

    co_return WSMessage::makeClose(_closeCode, _closeReason);
}

boost::asio::awaitable<WSMessage> WebSocketConnection::recv() {
    co_return co_await readMessage();
}

WebSocketHandler adaptWebSocketRoute(WebSocketRoute route) {
    return [route = std::move(route)](WebSocketConnection& ws,
                                      const HTTPRequest&   req) -> boost::asio::awaitable<void> {
        uint16_t    closeCode = 1000;
        std::string closeReason;
        try {
            if (route.onOpen) {
                route.onOpen(ws, req);
            }
            while (ws.isOpen()) {
                WSMessage msg = co_await ws.recv();
                if (msg.isClose()) {
                    closeCode = msg.closeCode();
                    closeReason.assign(msg.closeReason());
                    break;
                }
                if (route.onMessage) {
                    route.onMessage(ws, std::move(msg));
                }
            }
        } catch (...) {
            closeCode = ws.closeCode() != 1005 ? ws.closeCode() : static_cast<uint16_t>(1011);
            closeReason = ws.closeReason().empty() ? "internal error" : std::string(ws.closeReason());
        }

        if (route.onClose) {
            route.onClose(ws, closeCode, closeReason);
        }
        co_return;
    };
}

}  // namespace geruest
