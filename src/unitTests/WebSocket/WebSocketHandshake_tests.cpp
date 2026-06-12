/**
 * @file WebSocketHandshake_tests.cpp
 * @brief Unit tests for WebSocket HTTP upgrade detection and handshake response.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../../data/HTTPRequest.hpp"
#include "../../server/WebSocket.hpp"

using namespace geruest;

namespace {

HTTPRequest makeWsRequest(std::string raw) {
    return HTTPRequest(std::move(raw), "127.0.0.1", "/root");
}

std::string canonicalHandshake() {
    return "GET /chat HTTP/1.1\r\n"
           "Host: example.com\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
           "Sec-WebSocket-Version: 13\r\n"
           "\r\n";
}

}  // namespace

TEST(WebSocketHandshake, AcceptsCanonical) {
    const HTTPRequest req = makeWsRequest(canonicalHandshake());
    EXPECT_TRUE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, AcceptsConnectionUpgradeMixedCase) {
    std::string raw = "GET /chat HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: keep-alive, Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "\r\n";
    const HTTPRequest req = makeWsRequest(std::move(raw));
    EXPECT_TRUE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, RejectsPostMethod) {
    std::string raw = "POST /chat HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "\r\n";
    const HTTPRequest req = makeWsRequest(std::move(raw));
    EXPECT_FALSE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, RejectsMissingUpgrade) {
    std::string raw = "GET /chat HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "\r\n";
    const HTTPRequest req = makeWsRequest(std::move(raw));
    EXPECT_FALSE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, UpgradeIntentWithoutFullValidation) {
    std::string raw = "GET /chat HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Version: 8\r\n"
                      "\r\n";
    const HTTPRequest req = makeWsRequest(std::move(raw));
    EXPECT_TRUE(isWebSocketUpgradeIntent(req));
    EXPECT_FALSE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, NoUpgradeIntentForH2c) {
    std::string raw = "GET /api/ping HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Upgrade: h2c\r\n"
                      "Connection: Upgrade, HTTP2-Settings\r\n"
                      "\r\n";
    const HTTPRequest req = makeWsRequest(std::move(raw));
    EXPECT_FALSE(isWebSocketUpgradeIntent(req));
    EXPECT_FALSE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, RejectsWrongVersion) {
    std::string raw = "GET /chat HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 8\r\n"
                      "\r\n";
    const HTTPRequest req = makeWsRequest(std::move(raw));
    EXPECT_FALSE(isWebSocketUpgrade(req));
}

TEST(WebSocketHandshake, PickSubprotocolFirstClientMatch) {
    const std::vector<std::string> server = {"superchat", "chat"};
    EXPECT_EQ(pickSubprotocol("chat, superchat", server), "chat");
}

TEST(WebSocketHandshake, PickSubprotocolNoOverlap) {
    const std::vector<std::string> server = {"chat"};
    EXPECT_EQ(pickSubprotocol("xmpp", server), "");
}

TEST(WebSocketHandshake, BuildHandshakeResponseRequiredHeaders) {
    const std::string resp = buildHandshakeResponse("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    EXPECT_NE(resp.find("HTTP/1.1 101 Switching Protocols"), std::string::npos);
    EXPECT_NE(resp.find("Upgrade: websocket"), std::string::npos);
    EXPECT_NE(resp.find("Connection: Upgrade"), std::string::npos);
    EXPECT_NE(resp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), std::string::npos);
    EXPECT_EQ(resp.substr(resp.size() - 4), "\r\n\r\n");
}

TEST(WebSocketHandshake, BuildHandshakeResponseIncludesSubprotocol) {
    const std::string resp = buildHandshakeResponse("accept", "chat");
    EXPECT_NE(resp.find("Sec-WebSocket-Protocol: chat"), std::string::npos);
}
