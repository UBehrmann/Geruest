/**
 * @file ResponseWriter.hpp
 * @brief HTTP response serialization and socket writes (including sendfile).
 */

#ifndef GERUEST_RESPONSEWRITER_HPP
#define GERUEST_RESPONSEWRITER_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <string>

#include "data/HTTPRequest.hpp"

namespace geruest {

struct StaticCacheHeaders;

class Handler;

class ResponseWriter {
   public:
    explicit ResponseWriter(boost::asio::ip::tcp::socket& socket);

    boost::asio::awaitable<bool> sendSocketAsync(const char* bufferToSend, size_t size);
    boost::asio::awaitable<bool> sendFileBodyZeroCopyAsync(const std::string& contentPath, size_t fileSize);

    boost::asio::awaitable<void> sendResponseAsync(const std::string& status, const std::string& contentType,
                                                   const std::string& content,
                                                   const std::function<void(const std::string&)>& logError);

    boost::asio::awaitable<void> sendNotFoundResponseAsync(HTTPRequest* httpRequest, Handler& host);

    boost::asio::awaitable<void> sendServiceUnavailableResponseAsync(const std::string& why, Handler& host);

    boost::asio::awaitable<void> sendFileAsync(const std::string& contentType, const std::string& contentPath,
                                               HTTPRequest* httpRequest, Handler& host);

    boost::asio::awaitable<void> sendNotModifiedAsync(const StaticCacheHeaders& headers, Handler& host);

    boost::asio::awaitable<bool> sendBinaryFileHeaderAsync(const std::string& status, const std::string& contentType,
                                                           const StaticCacheHeaders& headers, size_t fileSize,
                                                           Handler& host);

    std::string& scratch() { return responseScratch_; }
    const std::string& scratch() const { return responseScratch_; }

   private:
    boost::asio::ip::tcp::socket& socket_;
    std::string                   responseScratch_;
};

}  // namespace geruest

#endif  // GERUEST_RESPONSEWRITER_HPP
