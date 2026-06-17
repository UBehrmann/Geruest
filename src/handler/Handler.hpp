/**
 * @file Handler.hpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief HTTP connection handler using Boost.Asio async I/O (C++20 coroutines).
 */

#ifndef GERUEST_HANDLER_HPP
#define GERUEST_HANDLER_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"

// Max packet size
#define BUFFER_SIZE 8192

namespace geruest {

class Handler {
   private:
    static unsigned clientCount;

    boost::asio::ip::tcp::socket& clientSocket;

    unsigned idling = 0;

    std::istringstream requestStream;

    unsigned int messageCount = 0;

    const ServerData& serverData;

    const std::string IP;

    std::unique_ptr<char[]> buffer;
    std::int64_t           bufferLength = 0;

    /** Unread bytes after the last fully parsed request (HTTP/1.1 pipelining / partial reads). */
    std::string pendingRequestData;

    bool _upgraded = false;

    /** False for /status and other monitoring paths excluded from metrics. */
    bool _countRequestInMetrics = true;

    /** Reused for HTTPResponse::serializeTo and similar to reduce per-send allocations. */
    std::string responseScratch_;

    void record4xxMetric() const;
    void record5xxMetric() const;
    void recordErrorMetric() const;

    /** Returns denial response when a page gate rejects access; empty when allowed or no gate. */
    std::optional<HTTPResponse> checkPageGateDenial(const HTTPRequest& request) const;

    /** Returns 403 when a route gate rejects access; empty when allowed or no gate. */
    std::optional<HTTPResponse> checkRouteGateDenial(const HTTPRequest& request) const;

    boost::asio::awaitable<bool> readSocketAsync(std::string_view phase = {});
    boost::asio::awaitable<bool> readSocketAsync(char* bufferToUse, size_t size, std::string_view phase = {});

    boost::asio::awaitable<bool> discardFromSocketAsync(size_t byteCount);

    boost::asio::awaitable<bool> sendSocketAsync(const char* bufferToSend, size_t size);
    boost::asio::awaitable<bool> sendFileBodyZeroCopyAsync(const std::string& contentPath, size_t fileSize);

    void sendToLogger(const std::string& message, LogLevel level = LogLevel::Info) const;

    void sendToLoggerPages(const std::string& message) const;

    void sendToLoggerAPI(const std::string& message) const;

    void sendToLoggerUser(const std::string& message) const;

    void sendToLoggerError(const std::string& message) const;

    boost::asio::awaitable<void> handleRequestAsync(HTTPRequest* request);

    boost::asio::awaitable<bool> tryHandleWebSocketAsync(HTTPRequest* request);

    boost::asio::awaitable<void> sendFileAsync(const std::string& contentType, const std::string& contentPath,
                                               HTTPRequest* httpRequest);

    boost::asio::awaitable<void> sendResponseAsync(const std::string& status, const std::string& contentType,
                                                   const std::string& content);

    boost::asio::awaitable<void> sendNotFoundResponseAsync(HTTPRequest* httpRequest);
    boost::asio::awaitable<void> sendServiceUnavailableResponseAsync(const std::string& why);

    std::string getExtension(const std::string& path) const;

    std::string buildPath(std::string& pathReceived, const std::string& Extension, HTTPRequest* httpRequest) const;

    static std::string getContentType(const std::string& extension);

   public:
    Handler(boost::asio::ip::tcp::socket& socket, std::string clientIP, const ServerData& serverDataRef);

    ~Handler();

    boost::asio::awaitable<void> runAsync();
};

}  // namespace geruest

#endif  // GERUEST_HANDLER_HPP
