/**
 * @file HttpFraming.cpp
 */

#include "HttpFraming.hpp"

#include "ResponseWriter.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <climits>
#include <cstring>
#include <sstream>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"

namespace {

[[nodiscard]] bool isExpectedKeepAliveTeardown(const boost::system::error_code& ec, std::string_view phase) {
    if (phase != "waiting_for_request") {
        return false;
    }
    return ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
           ec == boost::asio::error::broken_pipe;
}

[[nodiscard]] const char* socketReadFailureHint(const boost::system::error_code& ec, std::string_view phase,
                                                unsigned int connectionReq) {
    if (ec == boost::asio::error::eof) {
        if (phase == "waiting_for_request" && connectionReq > 1) {
            return "client closed idle keep-alive connection (usually harmless)";
        }
        return "client closed connection (end of stream)";
    }
    if (ec == boost::asio::error::connection_reset) {
        if (phase == "waiting_for_request") {
            return "peer reset connection while waiting for next request (often browser idle teardown)";
        }
        return "connection reset by peer";
    }
    if (ec == boost::asio::error::operation_aborted) {
        return "read operation aborted (server shutdown or cancelled I/O)";
    }
    if (ec == boost::asio::error::broken_pipe) {
        return "broken pipe (peer already gone)";
    }
    if (ec == boost::asio::error::not_connected) {
        return "socket is not connected";
    }
    return nullptr;
}

}  // namespace

namespace geruest {

HttpFraming::HttpFraming(boost::asio::ip::tcp::socket& socket, ErrorLogFn logError, WarnLogFn logWarning)
    : socket_(socket), logError_(std::move(logError)), logWarning_(std::move(logWarning)),
      buffer_(std::make_unique<char[]>(kBufferSize)) {}

boost::asio::awaitable<bool> HttpFraming::readSocketAsync(char* bufferToUse, size_t size, std::string_view phase) {
    if (size > INT_MAX) {
        logError_("Size of buffer is too big.");
        co_return false;
    }

    try {
        const std::size_t n =
            co_await socket_.async_read_some(boost::asio::buffer(bufferToUse, size), boost::asio::use_awaitable);
        bufferLength_ = static_cast<std::int64_t>(n);
    } catch (const boost::system::system_error& e) {
        if (!isExpectedKeepAliveTeardown(e.code(), phase)) {
            std::ostringstream oss;
            oss << "Error reading from socket";
            if (!phase.empty()) {
                oss << " phase=" << phase;
            }
            oss << " connection_req=" << messageCount_;
            oss << " socket_open=" << (socket_.is_open() ? "yes" : "no");
            oss << " error=[" << e.code().category().name() << ':' << e.code().value() << "] " << e.code().message();
            if (const char* hint = socketReadFailureHint(e.code(), phase, messageCount_)) {
                oss << " hint=" << hint;
            }
            logWarning_(oss.str());
        }
        co_return false;
    }

    if (bufferLength_ == 0) {
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> HttpFraming::readSocketAsync(std::string_view phase) {
    co_return co_await readSocketAsync(buffer_.get(), kBufferSize, phase);
}

boost::asio::awaitable<bool> HttpFraming::discardFromSocketAsync(size_t byteCount) {
    while (byteCount > 0) {
        const size_t chunk = std::min(byteCount, kBufferSize);
        if (!co_await readSocketAsync(buffer_.get(), chunk, "discarding_body")) {
            co_return false;
        }
        if (bufferLength_ <= 0) {
            co_return false;
        }
        const size_t got = static_cast<size_t>(bufferLength_);
        byteCount -= got;
    }
    co_return true;
}

boost::asio::awaitable<FramedRequest> HttpFraming::readNextRequestAsync(ResponseWriter& writer,
                                                                       size_t maxRequestsPerConnection) {
    FramedRequest result;
    if (maxRequestsPerConnection != 0 && messageCount_ >= maxRequestsPerConnection) {
        result.outcome = FramingOutcome::Stop;
        co_return result;
    }

    ++messageCount_;
    std::string raw = std::move(pendingRequestData_);
    pendingRequestData_.clear();

    if (raw.empty()) {
        if (!co_await readSocketAsync("waiting_for_request")) {
            result.outcome = FramingOutcome::Stop;
            co_return result;
        }
        if (bufferLength_ <= 0) {
            logError_("Invalid buffer length in run loop.");
            result.outcome = FramingOutcome::Stop;
            co_return result;
        }
        raw.assign(buffer_.get(), static_cast<size_t>(bufferLength_));
    }

    while (!splitHttpHeaders(raw).has_value()) {
        if (raw.size() >= kMaxHttpHeaderBytes) {
            logError_("HTTP headers exceed maximum size.");
            result.outcome = FramingOutcome::Abort;
            co_return result;
        }
        if (!co_await readSocketAsync("reading_headers")) {
            result.outcome = FramingOutcome::Abort;
            co_return result;
        }
        if (bufferLength_ <= 0) {
            logError_("Invalid buffer length while reading headers.");
            result.outcome = FramingOutcome::Abort;
            co_return result;
        }
        raw.append(buffer_.get(), static_cast<size_t>(bufferLength_));
    }

    const size_t headerEnd = splitHttpHeaders(raw)->headerSectionEnd;

    bool hasCL = false;
    bool hasChunked = false;
    size_t bodyExpected = 0;
    const std::string_view headerPrefix(raw.data(), headerEnd);
    const HeaderPreflight preflight = parseHeaderPreflight(headerPrefix);

    if (!preflight.expect.empty() && httpExpectIs100Continue(preflight.expect)) {
        static const char k100[] = "HTTP/1.1 100 Continue\r\n\r\n";
        if (!co_await writer.sendSocketAsync(k100, sizeof(k100) - 1)) {
            result.outcome = FramingOutcome::Abort;
            co_return result;
        }
    }

    if (!preflight.contentLength.empty()) {
        if (!parseContentLengthBytes(preflight.contentLength, &bodyExpected)) {
            HTTPResponse br = responseBadRequest(nullptr);
            br.serializeTo(writer.scratch());
            co_await writer.sendSocketAsync(writer.scratch().data(), writer.scratch().size());
            result.outcome = FramingOutcome::Abort;
            co_return result;
        }
        hasCL = true;
    }
    if (!preflight.transferEncoding.empty()) {
        hasChunked = httpConnectionHeaderHasToken(preflight.transferEncoding, "chunked");
    }

    if (hasCL && bodyExpected > kMaxHttpBodyBytes) {
        HTTPResponse br = responseBadRequest(nullptr);
        br.serializeTo(writer.scratch());
        co_await writer.sendSocketAsync(writer.scratch().data(), writer.scratch().size());
        const size_t already = raw.size() > headerEnd ? raw.size() - headerEnd : 0;
        const size_t remain = bodyExpected > already ? bodyExpected - already : 0;
        static_cast<void>(co_await discardFromSocketAsync(remain));
        result.outcome = FramingOutcome::Abort;
        co_return result;
    }

    size_t needTotal = headerEnd + (hasCL ? bodyExpected : 0);
    if (hasChunked) {
        while (true) {
            const size_t chunkEnd = findChunkedBodyEnd(raw, headerEnd);
            if (chunkEnd != std::string::npos) {
                needTotal = chunkEnd;
                break;
            }
            if (!co_await readSocketAsync("reading_chunked_body")) {
                result.outcome = FramingOutcome::Abort;
                co_return result;
            }
            if (bufferLength_ <= 0) {
                logError_("Invalid buffer length when reading chunked body.");
                result.outcome = FramingOutcome::Abort;
                co_return result;
            }
            raw.append(buffer_.get(), static_cast<size_t>(bufferLength_));
            if (raw.size() > kMaxHttpBodyBytes + headerEnd) {
                HTTPResponse br = responseBadRequest(nullptr);
                br.serializeTo(writer.scratch());
                co_await writer.sendSocketAsync(writer.scratch().data(), writer.scratch().size());
                result.outcome = FramingOutcome::Abort;
                co_return result;
            }
        }
    } else {
        while (raw.size() < needTotal) {
            if (!co_await readSocketAsync("reading_body")) {
                result.outcome = FramingOutcome::Abort;
                co_return result;
            }
            if (bufferLength_ <= 0) {
                logError_("Invalid buffer length when reading body.");
                result.outcome = FramingOutcome::Abort;
                co_return result;
            }
            raw.append(buffer_.get(), static_cast<size_t>(bufferLength_));
        }
    }

    result.messageBacking = std::make_shared<const std::string>(raw, 0, needTotal);
    if (raw.size() > needTotal) {
        pendingRequestData_.assign(raw.data() + needTotal, raw.size() - needTotal);
    } else {
        pendingRequestData_.clear();
    }

    std::memset(buffer_.get(), 0, kBufferSize);
    result.outcome = FramingOutcome::Request;
    co_return result;
}

}  // namespace geruest
