/**
 * @file HttpFraming.hpp
 * @brief Read and assemble complete HTTP/1.x request messages from a socket.
 */

#ifndef GERUEST_HTTPFRAMING_HPP
#define GERUEST_HTTPFRAMING_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace geruest {

class ResponseWriter;

enum class FramingOutcome { Request, Stop, Abort };

struct FramedRequest {
    FramingOutcome outcome = FramingOutcome::Stop;
    std::shared_ptr<const std::string> messageBacking;
};

/** Socket reads + header/body assembly for one HTTP request per call. */
class HttpFraming {
   public:
    static constexpr size_t kBufferSize = 8192;

    using ErrorLogFn = std::function<void(const std::string&)>;
    using WarnLogFn = std::function<void(const std::string&)>;

    HttpFraming(boost::asio::ip::tcp::socket& socket, ErrorLogFn logError, WarnLogFn logWarning);

    unsigned int messageCount() const { return messageCount_; }

    /** Reads until one full request is available, the connection ends, or framing aborts. */
    boost::asio::awaitable<FramedRequest> readNextRequestAsync(ResponseWriter& writer,
                                                               size_t maxRequestsPerConnection);

   private:
    boost::asio::awaitable<bool> readSocketAsync(char* bufferToUse, size_t size, std::string_view phase);
    boost::asio::awaitable<bool> readSocketAsync(std::string_view phase);
    boost::asio::awaitable<bool> discardFromSocketAsync(size_t byteCount);

    boost::asio::ip::tcp::socket& socket_;
    ErrorLogFn                   logError_;
    WarnLogFn                    logWarning_;

    std::unique_ptr<char[]> buffer_;
    std::int64_t            bufferLength_ = 0;
    std::string             pendingRequestData_;
    unsigned int            messageCount_ = 0;
};

}  // namespace geruest

#endif  // GERUEST_HTTPFRAMING_HPP
