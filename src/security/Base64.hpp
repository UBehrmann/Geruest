/**
 * @file Base64.hpp
 * @brief RFC 4648 base64 encode/decode helpers.
 */

#ifndef GERUEST_BASE64_HPP
#define GERUEST_BASE64_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace geruest {

inline const char* base64Alphabet() {
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
           "abcdefghijklmnopqrstuvwxyz"
           "0123456789+/";
}

inline std::string base64Encode(std::string_view input) {
    static const char* const kAlphabet = base64Alphabet();
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < input.size()) {
        const uint32_t b0 = static_cast<uint8_t>(input[i]);
        const uint32_t b1 = (i + 1 < input.size()) ? static_cast<uint8_t>(input[i + 1]) : 0;
        const uint32_t b2 = (i + 2 < input.size()) ? static_cast<uint8_t>(input[i + 2]) : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3F]);

        if (i + 1 < input.size()) {
            out.push_back(kAlphabet[(triple >> 6) & 0x3F]);
        } else {
            out.push_back('=');
        }

        if (i + 2 < input.size()) {
            out.push_back(kAlphabet[triple & 0x3F]);
        } else {
            out.push_back('=');
        }

        i += 3;
    }
    return out;
}

inline std::string base64Decode(std::string_view encoded) {
    static const std::string kAlphabet = base64Alphabet();
    std::string              decoded;
    std::vector<int>         temp(4);

    size_t i = 0;
    const size_t len = encoded.size();

    while (i < len && encoded[i] != '=') {
        size_t j = 0;
        while (j < 4 && i < len && encoded[i] != '=') {
            const size_t pos = kAlphabet.find(encoded[i]);
            if (pos == std::string::npos) {
                ++i;
                continue;
            }
            temp[j] = static_cast<int>(pos);
            ++j;
            ++i;
        }

        if (j >= 2) {
            decoded += static_cast<char>((temp[0] << 2) + ((temp[1] & 0x30) >> 4));
        }
        if (j >= 3) {
            decoded += static_cast<char>(((temp[1] & 0xf) << 4) + ((temp[2] & 0x3c) >> 2));
        }
        if (j >= 4) {
            decoded += static_cast<char>(((temp[2] & 0x3) << 6) + temp[3]);
        }
    }

    return decoded;
}

}  // namespace geruest

#endif  // GERUEST_BASE64_HPP
