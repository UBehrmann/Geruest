/**
 * @file WebSocketIntegration_tests.cpp
 * @brief End-to-end WebSocket tests over loopback TCP.
 */

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "Geruest.hpp"
#include "server/WebSocket.hpp"

using namespace geruest;

namespace {

int connectTo(int port) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
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

std::string readUnmaskedTextPayload(int fd) {
    std::string raw;
    if (!recvSome(fd, raw, 4)) {
        return {};
    }
    const size_t payloadLen = static_cast<unsigned char>(raw[1]) & 0x7F;
    if (!recvSome(fd, raw, 2 + payloadLen)) {
        return {};
    }
    return raw.substr(2, payloadLen);
}

void startServerOnBackground(Geruest& server) {
    server.setPort(0);
    server.setWorkerThreadCount(1);
    server.setMaxQueueSize(8);
    server.init();
    std::thread([&server] { server.start(); }).detach();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

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

    startServerOnBackground(server);
    const int port = server.getListenPort();
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

    const std::string echoed = readUnmaskedTextPayload(fd);
    EXPECT_EQ(echoed, "hello");

    close(fd);
    server.stop();
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

    startServerOnBackground(server);
    const int fd = connectTo(server.getListenPort());
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
    const std::string echoed = readUnmaskedTextPayload(fd);
    EXPECT_EQ(echoed, "ping");

    close(fd);
    server.stop();
    EXPECT_GE(messageCount.load(), 1);
}

TEST(WebSocketIntegration, RejectsBadVersion) {
    Geruest server;
    server.addRouteWebSocket(
        "/echo",
        [](WebSocketConnection&, const HTTPRequest&) -> boost::asio::awaitable<void> { co_return; });

    startServerOnBackground(server);
    const int fd = connectTo(server.getListenPort());
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
    server.stop();
}

TEST(WebSocketIntegration, RouteNotFoundReturns404) {
    Geruest server;
    startServerOnBackground(server);
    const int fd = connectTo(server.getListenPort());
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
    server.stop();
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

    startServerOnBackground(server);
    const int fd = connectTo(server.getListenPort());
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
    server.stop();
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

    startServerOnBackground(server);
    const int fd = connectTo(server.getListenPort());
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

    std::string payload;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (payload.find('\x88') == std::string::npos && std::chrono::steady_clock::now() < deadline) {
        char buf[512];
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            payload.append(buf, static_cast<size_t>(n));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    EXPECT_NE(payload.find("bye"), std::string::npos);
    EXPECT_NE(payload.find('\x88'), std::string::npos);

    close(fd);
    server.stop();
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

    startServerOnBackground(server);
    const int fd = connectTo(server.getListenPort());
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
    server.stop();
}
