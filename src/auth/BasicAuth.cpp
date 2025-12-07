/**
 * @file BasicAuth.cpp
 * @date 07.12.2025
 *
 * @author Urs Behrmann
 *
 * @brief Implementation of Basic HTTP Authentication system
 */

#include "auth/BasicAuth.hpp"
#include <algorithm>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cstring>
#include <cstdint>

namespace geruest {

// Base64 decoding lookup table
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string BasicAuth::base64Decode(const std::string& encoded) {
    std::string decoded;
    std::vector<int> temp(4);
    
    size_t i = 0;
    size_t len = encoded.length();
    
    while (i < len && encoded[i] != '=') {
        // Collect 4 base64 characters
        size_t j = 0;
        while (j < 4 && i < len && encoded[i] != '=') {
            size_t pos = base64_chars.find(encoded[i]);
            if (pos == std::string::npos) {
                // Invalid character, skip it
                i++;
                continue;
            }
            temp[j] = static_cast<int>(pos);
            j++;
            i++;
        }
        
        // Decode the collected characters
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

std::string BasicAuth::sha256Hash(const std::string& input) {
    // SHA-256 constants
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    
    // Initial hash values
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    // Prepare message
    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t msgBitLen = msg.size() * 8;
    
    // Padding
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    
    // Append length
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((msgBitLen >> (i * 8)) & 0xff));
    }
    
    // Process message in 512-bit chunks
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[64];
        
        // Copy chunk into first 16 words of message schedule
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }
        
        // Extend the first 16 words into the remaining 48 words
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25)) ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);
            uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15)) ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        
        // Initialize working variables
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        
        // Compression function main loop
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            
            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        
        // Add compressed chunk to current hash value
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    
    // Produce final hash value
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        result << std::setw(8) << h[i];
    }
    
    return result.str();
}

std::string BasicAuth::hashPassword(const std::string& password) {
    return sha256Hash(password);
}

bool BasicAuth::parseAuthorizationHeader(const std::string& authHeader, 
                                         std::string& username, 
                                         std::string& password) const {
    // Expected format: "Basic base64encodedcredentials"
    if (authHeader.empty()) {
        return false;
    }
    
    // Find "Basic " prefix (case-insensitive)
    size_t basicPos = authHeader.find("Basic ");
    if (basicPos == std::string::npos) {
        basicPos = authHeader.find("basic ");
    }
    if (basicPos == std::string::npos) {
        return false;
    }
    
    // Extract base64 encoded part
    std::string encoded = authHeader.substr(basicPos + 6);
    
    // Remove any whitespace
    encoded.erase(std::remove_if(encoded.begin(), encoded.end(), ::isspace), encoded.end());
    
    if (encoded.empty()) {
        return false;
    }
    
    // Decode base64
    std::string decoded = base64Decode(encoded);
    
    // Split by colon to get username:password
    size_t colonPos = decoded.find(':');
    if (colonPos == std::string::npos) {
        return false;
    }
    
    username = decoded.substr(0, colonPos);
    password = decoded.substr(colonPos + 1);
    
    return !username.empty();
}

BasicAuth::BasicAuth() : _enabled(false) {}

void BasicAuth::setEnabled(bool enabled) {
    _enabled = enabled;
}

bool BasicAuth::isEnabled() const {
    return _enabled;
}

void BasicAuth::addUser(const std::string& username, const std::string& password) {
    if (!username.empty()) {
        _users[username] = sha256Hash(password);
    }
}

void BasicAuth::addUserHashed(const std::string& username, const std::string& hashedPassword) {
    if (!username.empty()) {
        _users[username] = hashedPassword;
    }
}

bool BasicAuth::removeUser(const std::string& username) {
    auto it = _users.find(username);
    if (it != _users.end()) {
        _users.erase(it);
        return true;
    }
    return false;
}

bool BasicAuth::hasUsers() const {
    return !_users.empty();
}

size_t BasicAuth::userCount() const {
    return _users.size();
}

void BasicAuth::clearUsers() {
    _users.clear();
}

void BasicAuth::addProtectedPage(const std::string& path) {
    if (!path.empty()) {
        _protectedPages.insert(path);
    }
}

bool BasicAuth::removeProtectedPage(const std::string& path) {
    auto it = _protectedPages.find(path);
    if (it != _protectedPages.end()) {
        _protectedPages.erase(it);
        return true;
    }
    return false;
}

bool BasicAuth::isPageProtected(const std::string& path) const {
    return _protectedPages.find(path) != _protectedPages.end();
}

size_t BasicAuth::protectedPageCount() const {
    return _protectedPages.size();
}

void BasicAuth::clearProtectedPages() {
    _protectedPages.clear();
}

bool BasicAuth::verifyCredentials(const std::string& authHeader) const {
    std::string username, password;
    
    if (!parseAuthorizationHeader(authHeader, username, password)) {
        return false;
    }
    
    // Find user in credentials map
    auto it = _users.find(username);
    if (it == _users.end()) {
        return false;
    }
    
    // Hash the provided password and compare with stored hash
    return it->second == sha256Hash(password);
}

bool BasicAuth::requiresAuth(const std::string& path) const {
    // Authentication not required if:
    // 1. Authentication is disabled globally
    if (!_enabled) {
        return false;
    }
    
    // 2. No users are configured (no one to authenticate against)
    if (!hasUsers()) {
        return false;
    }
    
    // 3. Page is not in protected list
    if (!isPageProtected(path)) {
        return false;
    }
    
    return true;
}

bool BasicAuth::authenticate(const std::string& path, const std::string& authHeader) const {
    // If authentication is not required for this path, grant access
    if (!requiresAuth(path)) {
        return true;
    }
    
    // Authentication is required, verify credentials
    return verifyCredentials(authHeader);
}

}  // namespace geruest
