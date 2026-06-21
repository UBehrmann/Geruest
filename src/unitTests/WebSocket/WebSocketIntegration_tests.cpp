/**
 * @file WebSocketIntegration_tests.cpp
 * @brief End-to-end WebSocket tests over loopback TCP.
 */

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Geruest.hpp"
#include "server/WebSocket.hpp"

using namespace geruest;

namespace {

bool setSocketTimeouts(int fd, int timeoutMs) {
    timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000) * 1000);
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

void wakeAcceptOnLocalhost(int port) {
    if (port <= 0 || port > 65535) {
        return;
    }
    const int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) {
        return;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    (void)::connect(c, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    close(c);
}

int connectTo(int port, int timeoutMs = 5000) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (!setSocketTimeouts(fd, timeoutMs)) {
        close(fd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return -1;
    }

    const int connectRc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (connectRc != 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    if (connectRc != 0) {
        pollfd pfd{fd, POLLOUT, 0};
        const int pollRc = poll(&pfd, 1, timeoutMs);
        if (pollRc <= 0) {
            close(fd);
            return -1;
        }
        int sockErr = 0;
        socklen_t errLen = sizeof(sockErr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockErr, &errLen) != 0 || sockErr != 0) {
            close(fd);
            return -1;
        }
    }

    if (fcntl(fd, F_SETFL, flags) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool sendAll(int fd, const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
        const ssize_t n = ::send(fd, p + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recvSome(int fd, std::string& out, size_t minBytes, int timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (out.size() < minBytes && std::chrono::steady_clock::now() < deadline) {
        char buf[4096];
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        out.append(buf, static_cast<size_t>(n));
    }
    return out.size() >= minBytes;
}

std::vector<uint8_t> maskedTextFrame(std::string_view text) {
    const uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    std::vector<uint8_t> frame;
    frame.push_back(0x81);
    frame.push_back(static_cast<uint8_t>(0x80 | text.size()));
    frame.insert(frame.end(), mask, mask + 4);
    for (size_t i = 0; i < text.size(); ++i) {
        frame.push_back(static_cast<uint8_t>(text[i] ^ mask[i % 4]));
    }
    return frame;
}

class SocketRecvBuffer {
public:
    explicit SocketRecvBuffer(int fd) : fd_(fd) {}

    struct ServerFrame {
        uint8_t     opcode{0};
        std::string payload;
    };

    std::optional<ServerFrame> popServerFrame(int timeoutMs = 3000) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true) {
            if (const auto frameLen = completeServerFrameLength()) {
                return extractFrame(*frameLen);
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return std::nullopt;
            }
            const int remainingMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                          deadline - std::chrono::steady_clock::now())
                                                          .count());
            if (remainingMs <= 0) {
                return std::nullopt;
            }
            const size_t need = buf_.size() + 1;
            if (!recvSome(fd_, buf_, need, remainingMs) && !completeServerFrameLength()) {
                return std::nullopt;
            }
        }
    }

private:
    std::optional<size_t> completeServerFrameLength() const {
        if (buf_.size() < 2) {
            return std::nullopt;
        }
        if ((static_cast<uint8_t>(buf_[1]) & 0x80) != 0) {
            return std::nullopt;
        }

        uint64_t payloadLen = static_cast<uint8_t>(buf_[1]) & 0x7F;
        size_t   header     = 2;
        if (payloadLen == 126) {
            if (buf_.size() < 4) {
                return std::nullopt;
            }
            payloadLen = (static_cast<uint64_t>(static_cast<uint8_t>(buf_[2])) << 8) |
                         static_cast<uint8_t>(buf_[3]);
            header = 4;
        } else if (payloadLen == 127) {
            if (buf_.size() < 10) {
                return std::nullopt;
            }
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | static_cast<uint8_t>(buf_[2 + static_cast<size_t>(i)]);
            }
            header = 10;
        }

        const size_t total = header + static_cast<size_t>(payloadLen);
        if (buf_.size() < total) {
            return std::nullopt;
        }
        return total;
    }

    ServerFrame extractFrame(size_t frameLen) {
        uint64_t payloadLen = static_cast<uint8_t>(buf_[1]) & 0x7F;
        size_t   header     = 2;
        if (payloadLen == 126) {
            payloadLen = (static_cast<uint64_t>(static_cast<uint8_t>(buf_[2])) << 8) |
                         static_cast<uint8_t>(buf_[3]);
            header = 4;
        } else if (payloadLen == 127) {
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | static_cast<uint8_t>(buf_[2 + static_cast<size_t>(i)]);
            }
            header = 10;
        }

        ServerFrame frame;
        frame.opcode   = static_cast<uint8_t>(buf_[0]) & 0x0F;
        frame.payload  = buf_.substr(header, static_cast<size_t>(payloadLen));
        buf_.erase(0, frameLen);
        return frame;
    }

    int         fd_{-1};
    std::string buf_;
};

std::optional<std::string> readServerTextPayload(SocketRecvBuffer& rx, int timeoutMs = 3000) {
    const std::optional<SocketRecvBuffer::ServerFrame> frame = rx.popServerFrame(timeoutMs);
    if (!frame || frame->opcode != 0x1) {
        return std::nullopt;
    }
    return frame->payload;
}

class ScopedBackgroundServer {
public:
    ~ScopedBackgroundServer() { shutdown(); }

    void launch(Geruest& server) {
        shutdown();

        const auto tag = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        metricsDir_ = std::filesystem::temp_directory_path() / ("geruest_ws_int_" + tag);
        std::filesystem::create_directories(metricsDir_);

        server_ = &server;
        server.setPort(0);
        server.setWorkerThreadCount(1);
        server.setMaxQueueSize(8);
        server.setStatusPersistencePath((metricsDir_ / "metrics.json").string());
        ASSERT_TRUE(server.init());
        listenPort_ = server.getListenPort();
        ASSERT_GT(listenPort_, 0);

        thread_ = std::thread([&server] { server.start(); });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!server.isRunning() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ASSERT_TRUE(server.isRunning()) << "background server failed to start";
    }

    int listenPort() const { return listenPort_; }

    void shutdown() {
        if (server_ != nullptr && server_->isRunning()) {
            server_->stop();
            wakeAcceptOnLocalhost(listenPort_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        server_     = nullptr;
        listenPort_ = -1;
        if (!metricsDir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(metricsDir_, ec);
            metricsDir_.clear();
        }
    }

private:
    Geruest*                 server_{nullptr};
    std::thread              thread_;
    int                      listenPort_{-1};
    std::filesystem::path    metricsDir_;
};

}  // namespace

TEST(WebSocketIntegration, CoroutineEcho) {
    Geruest server;
    server.addRouteWebSocket(
        "/echo",
        [](WebSocketConnection& ws, const HTTPRequest&) -> boost::asio::awaitable<void> {
            WSMessage msg = co_await ws.recv();
            if (msg.isText()) {
                co_await ws.send(msg.text());
            }
            co_return;
        });

    ScopedBackgroundServer bg;
    bg.launch(server);
    const int port = bg.listenPort();
    ASSERT_GT(port, 0);

    const int fd = connectTo(port);
    ASSERT_GE(fd, 0);

    const std::string handshake =
        "GET /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, handshake.data(), handshake.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));
    EXPECT_NE(response.find("101 Switching Protocols"), std::string::npos);

    const auto frame = maskedTextFrame("hello");
    ASSERT_TRUE(sendAll(fd, frame.data(), frame.size()));

    SocketRecvBuffer rx(fd);
    const std::optional<std::string> echoed = readServerTextPayload(rx);
    ASSERT_TRUE(echoed.has_value());
    EXPECT_EQ(*echoed, "hello");

    close(fd);
}

TEST(WebSocketIntegration, CallbackEcho) {
    Geruest server;
    WebSocketRoute route;
    std::atomic<int> messageCount{0};
    route.onMessage = [&messageCount](WebSocketConnection& ws, WSMessage msg) {
        if (msg.isText()) {
            ++messageCount;
            ws.sendNow(msg.text());
        }
    };
    server.addRouteWebSocket("/echo-cb", route);

    ScopedBackgroundServer bg;
    bg.launch(server);
    const int fd = connectTo(bg.listenPort());
    ASSERT_GE(fd, 0);

    const std::string handshake =
        "GET /echo-cb HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, handshake.data(), handshake.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));

    const auto frame = maskedTextFrame("ping");
    ASSERT_TRUE(sendAll(fd, frame.data(), frame.size()));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SocketRecvBuffer rx(fd);
    const std::optional<std::string> echoed = readServerTextPayload(rx);
    ASSERT_TRUE(echoed.has_value());
    EXPECT_EQ(*echoed, "ping");

    close(fd);
    EXPECT_GE(messageCount.load(), 1);
}

TEST(WebSocketIntegration, RejectsBadVersion) {
    Geruest server;
    server.addRouteWebSocket(
        "/echo",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });

    ScopedBackgroundServer bg;
    bg.launch(server);
    const int fd = connectTo(bg.listenPort());
    ASSERT_GE(fd, 0);

    const std::string handshake =
        "GET /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 8\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, handshake.data(), handshake.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));
    EXPECT_NE(response.find("400 Bad Request"), std::string::npos);

    close(fd);
}

TEST(WebSocketIntegration, RouteNotFoundReturns404) {
    Geruest server;
    ScopedBackgroundServer bg;
    bg.launch(server);
    const int fd = connectTo(bg.listenPort());
    ASSERT_GE(fd, 0);

    const std::string handshake =
        "GET /missing-ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, handshake.data(), handshake.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));
    EXPECT_NE(response.find("404 Not Found"), std::string::npos);

    close(fd);
}

TEST(WebSocketIntegration, NonWebSocketUpgradeReachesHttpRoute) {
    Geruest server;
    server.addRouteWebSocket(
        "/ws",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    server.addRoute("/api/ping", [](const HTTPRequest&) {
        HTTPResponse r("200 OK");
        r.setHeader("Content-Type", "text/plain");
        r.setBody("pong");
        return r;
    });

    ScopedBackgroundServer bg;
    bg.launch(server);
    const int fd = connectTo(bg.listenPort());
    ASSERT_GE(fd, 0);

    const std::string request =
        "GET /api/ping HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: h2c\r\n"
        "Connection: Upgrade, HTTP2-Settings\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, request.data(), request.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));
    EXPECT_NE(response.find("200 OK"), std::string::npos);
    EXPECT_NE(response.find("pong"), std::string::npos);

    close(fd);
}

TEST(WebSocketIntegration, ServerSendsCloseFrameAfterHandler) {
    Geruest server;
    server.addRouteWebSocket(
        "/echo",
        [](WebSocketConnection& ws, const HTTPRequest&) -> boost::asio::awaitable<void> {
            WSMessage msg = co_await ws.recv();
            if (msg.isText()) {
                co_await ws.send(msg.text());
            }
            co_return;
        });

    ScopedBackgroundServer bg;
    bg.launch(server);
    const int fd = connectTo(bg.listenPort());
    ASSERT_GE(fd, 0);

    const std::string handshake =
        "GET /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, handshake.data(), handshake.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));
    EXPECT_NE(response.find("101 Switching Protocols"), std::string::npos);

    const auto frame = maskedTextFrame("bye");
    ASSERT_TRUE(sendAll(fd, frame.data(), frame.size()));

    SocketRecvBuffer rx(fd);
    const std::optional<std::string> echoed = readServerTextPayload(rx);
    ASSERT_TRUE(echoed.has_value());
    EXPECT_EQ(*echoed, "bye");

    const std::optional<SocketRecvBuffer::ServerFrame> closeFrame = rx.popServerFrame(5000);
    ASSERT_TRUE(closeFrame.has_value());
    EXPECT_EQ(closeFrame->opcode, 0x8u);

    close(fd);
}

TEST(WebSocketIntegration, NormalHttpStillWorks) {
    Geruest server;
    server.addRouteWebSocket(
        "/ws",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });
    server.addRoute("/api/ping", [](const HTTPRequest&) {
        HTTPResponse r("200 OK");
        r.setHeader("Content-Type", "text/plain");
        r.setBody("pong");
        return r;
    });

    ScopedBackgroundServer bg;
    bg.launch(server);
    const int fd = connectTo(bg.listenPort());
    ASSERT_GE(fd, 0);

    const std::string request =
        "GET /api/ping HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";
    ASSERT_TRUE(sendAll(fd, request.data(), request.size()));

    std::string response;
    ASSERT_TRUE(recvSome(fd, response, 12));
    EXPECT_NE(response.find("200 OK"), std::string::npos);
    EXPECT_NE(response.find("pong"), std::string::npos);

    close(fd);
}
