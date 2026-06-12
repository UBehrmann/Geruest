/**
 * @file server/WebSocket.hpp
 * @brief WebSocket handshake helpers, message types, and connection API.
 */

#ifndef GERUEST_WEBSOCKET_HPP
#define GERUEST_WEBSOCKET_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "data/HTTPRequest.hpp"
#include "server/WebSocketTypes.hpp"

namespace geruest {

class WSMessage {
   public:
    WSOpcode opcode() const noexcept { return _opcode; }
    bool     isText() const noexcept { return _opcode == WSOpcode::Text; }
    bool     isBinary() const noexcept { return _opcode == WSOpcode::Binary; }
    bool     isClose() const noexcept { return _opcode == WSOpcode::Close; }
    bool     isPing() const noexcept { return _opcode == WSOpcode::Ping; }
    bool     isPong() const noexcept { return _opcode == WSOpcode::Pong; }

    std::string_view text() const noexcept {
        return std::string_view(reinterpret_cast<const char*>(_payload.data()), _payload.size());
    }
    const std::vector<uint8_t>& data() const noexcept { return _payload; }
    std::vector<uint8_t>        takeData() && { return std::move(_payload); }

    uint16_t         closeCode() const noexcept { return _closeCode; }
    std::string_view closeReason() const noexcept { return _closeReason; }

    static WSMessage makeText(std::string_view text);
    static WSMessage makeBinary(std::span<const uint8_t> data);
    static WSMessage makeClose(uint16_t code, std::string_view reason);

    static WSMessage makePing(std::vector<uint8_t> payload);
    static WSMessage makePong(std::vector<uint8_t> payload);

   private:
    friend class WebSocketConnection;

    WSOpcode             _opcode = WSOpcode::Text;
    std::vector<uint8_t> _payload;
    uint16_t             _closeCode = 1005;
    std::string          _closeReason;
};

class WebSocketConnection {
   public:
    using tcp_socket = boost::asio::ip::tcp::socket;

    WebSocketConnection(tcp_socket& sock, std::string clientIp, std::string selectedSubprotocol,
                        WebSocketLimits limits);

    boost::asio::awaitable<WSMessage> recv();
    boost::asio::awaitable<void>      send(std::string_view text);
    boost::asio::awaitable<void>      sendBinary(std::span<const uint8_t> data);
    boost::asio::awaitable<void>      ping(std::span<const uint8_t> payload = {});
    boost::asio::awaitable<void>      pong(std::span<const uint8_t> payload = {});
    boost::asio::awaitable<void>      close(uint16_t code = 1000, std::string_view reason = "");

    void sendNow(std::string_view text);
    void sendBinaryNow(std::span<const uint8_t> data);

    bool               isOpen() const noexcept { return _open; }
    uint16_t           closeCode() const noexcept { return _closeCode; }
    std::string_view   closeReason() const noexcept { return _closeReason; }
    const std::string& clientIp() const noexcept { return _clientIp; }
    const std::string& selectedSubprotocol() const noexcept { return _selectedSubprotocol; }

   private:
    tcp_socket&                                    _socket;
    boost::asio::strand<tcp_socket::executor_type> _strand;
    std::string                                    _clientIp;
    std::string                                    _selectedSubprotocol;
    WebSocketLimits                                _limits;

    bool        _open = true;
    uint16_t    _closeCode = 1005;
    std::string _closeReason;

    boost::asio::awaitable<void> writeFrame(WSOpcode op, std::span<const uint8_t> payload, bool fin = true);
    boost::asio::awaitable<void> writeRawFrame(std::vector<uint8_t> frame);
    boost::asio::awaitable<WSMessage> readMessage();
    boost::asio::awaitable<void>      protocolError(uint16_t code, std::string_view reason);
    void                              markClosed(uint16_t code, std::string_view reason);
};

struct WebSocketRoute {
    std::function<void(WebSocketConnection&, const HTTPRequest&)>         onOpen;
    std::function<void(WebSocketConnection&, WSMessage)>                  onMessage;
    std::function<void(WebSocketConnection&, uint16_t, std::string_view)> onClose;
};

using WebSocketHandler =
    std::function<boost::asio::awaitable<void>(WebSocketConnection&, const HTTPRequest&)>;

WebSocketHandler adaptWebSocketRoute(WebSocketRoute route);

// Handshake helpers (Handler + unit tests)
[[nodiscard]] bool        isWebSocketUpgrade(const HTTPRequest& req);
[[nodiscard]] std::string computeAcceptKey(std::string_view secKey);
[[nodiscard]] std::string buildHandshakeResponse(std::string_view acceptKey,
                                                 std::string_view subprotocol = {});
[[nodiscard]] std::string pickSubprotocol(std::string_view clientHeader,
                                          const std::vector<std::string>& serverOffered);

// Codec helpers exposed for unit tests
namespace websocket_codec {

[[nodiscard]] std::string sha1Hash(std::string_view input);
[[nodiscard]] std::vector<uint8_t> encodeServerFrame(WSOpcode opcode, std::span<const uint8_t> payload,
                                                      bool fin = true);
[[nodiscard]] WSMessage decodeClientFrame(std::span<const uint8_t> bytes, const WebSocketLimits& limits,
                                        std::span<const uint8_t>* remainder = nullptr);

}  // namespace websocket_codec

}  // namespace geruest

#endif  // GERUEST_WEBSOCKET_HPP
