/**
 * @file WebSocketCodec_tests.cpp
 * @brief Unit tests for WebSocket crypto and frame codec helpers.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "../../security/Base64.hpp"
#include "../../server/WebSocket.hpp"

using namespace geruest;
using namespace geruest::websocket_codec;

namespace {

std::vector<uint8_t> encodeMaskedClientFrame(WSOpcode opcode, std::span<const uint8_t> payload, bool fin = true,
                                             const uint8_t mask[4] = nullptr) {
    static const uint8_t kDefaultMask[4] = {0x37, 0xFA, 0x21, 0x3D};
    if (mask == nullptr) {
        mask = kDefaultMask;
    }

    std::vector<uint8_t> frame;
    uint8_t b0 = static_cast<uint8_t>(opcode) & 0x0F;
    if (fin) {
        b0 |= 0x80;
    }
    frame.push_back(b0);

    const size_t len = payload.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(0x80 | len));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<uint8_t>(0x80 | 126));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(static_cast<uint8_t>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<uint8_t>((len >> shift) & 0xFF));
        }
    }

    frame.insert(frame.end(), mask, mask + 4);
    for (size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(static_cast<uint8_t>(payload[i] ^ mask[i % 4]));
    }
    return frame;
}

}  // namespace

TEST(WebSocketCodec, Sha1Empty) {
    const std::string digest = sha1Hash("");
    EXPECT_EQ(digest.size(), 20U);
    std::string hex;
    for (unsigned char c : digest) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", c);
        hex += buf;
    }
    EXPECT_EQ(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(WebSocketCodec, Sha1Abc) {
    const std::string digest = sha1Hash("abc");
    std::string hex;
    for (unsigned char c : digest) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", c);
        hex += buf;
    }
    EXPECT_EQ(hex, "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(WebSocketCodec, Base64RfcVectors) {
    EXPECT_EQ(base64Encode("f"), "Zg==");
    EXPECT_EQ(base64Encode("fo"), "Zm8=");
    EXPECT_EQ(base64Encode("foo"), "Zm9v");
    EXPECT_EQ(base64Encode("foob"), "Zm9vYg==");
}

TEST(WebSocketCodec, Base64RoundTrip) {
    std::string input;
    for (int i = 0; i < 256; ++i) {
        input.push_back(static_cast<char>(i));
    }
    EXPECT_EQ(base64Decode(base64Encode(input)), input);
}

TEST(WebSocketCodec, AcceptKeyRfcExample) {
    EXPECT_EQ(computeAcceptKey("dGhlIHNhbXBsZSBub25jZQ=="), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(WebSocketCodec, EncodeServerTextHi) {
    const auto frame = encodeServerFrame(WSOpcode::Text, std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>("hi"), 2});
    ASSERT_EQ(frame.size(), 4U);
    EXPECT_EQ(frame[0], 0x81);
    EXPECT_EQ(frame[1], 0x02);
    EXPECT_EQ(frame[2], 'h');
    EXPECT_EQ(frame[3], 'i');
}

TEST(WebSocketCodec, DecodeMaskedTextHi) {
    WebSocketLimits limits;
  const std::array<uint8_t, 2> payload = {'H', 'i'};
    const auto bytes = encodeMaskedClientFrame(WSOpcode::Text, payload);
    const WSMessage msg = decodeClientFrame(bytes, limits);
    EXPECT_TRUE(msg.isText());
    EXPECT_EQ(msg.text(), "Hi");
}

TEST(WebSocketCodec, RejectsUnmaskedClientFrame) {
    WebSocketLimits limits;
    const std::vector<uint8_t> bytes = {0x81, 0x02, 'h', 'i'};
    EXPECT_THROW({ (void)decodeClientFrame(bytes, limits); }, std::runtime_error);
}

TEST(WebSocketCodec, DecodeCloseFrame) {
    WebSocketLimits limits;
    std::vector<uint8_t> payload = {0x03, 0xE9};
    payload.insert(payload.end(), {'b', 'y', 'e'});
    const auto bytes = encodeMaskedClientFrame(WSOpcode::Close, payload);
    const WSMessage msg = decodeClientFrame(bytes, limits);
    EXPECT_TRUE(msg.isClose());
    EXPECT_EQ(msg.closeCode(), 1001);
    EXPECT_EQ(msg.closeReason(), "bye");
}

TEST(WebSocketCodec, RejectsOversizedFrame) {
    WebSocketLimits limits;
    limits.maxFrameBytes = 8;
    std::vector<uint8_t> payload(16, 'x');
    const auto bytes = encodeMaskedClientFrame(WSOpcode::Text, payload);
    EXPECT_THROW({ (void)decodeClientFrame(bytes, limits); }, std::runtime_error);
}
