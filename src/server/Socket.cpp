/**
 * @file server/Socket.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Boost.Asio acceptor, init/start/stop, and /status persistence loop.
 */

#include "../Geruest.hpp"
#include "HttpSession.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <cstring>
#include <thread>

namespace geruest {

namespace {
constexpr const char* kOverloadedResponse =
    "HTTP/1.1 503 Service Unavailable\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 19\r\n"
    "Connection: close\r\n"
    "Retry-After: 1\r\n"
    "\r\n"
    "Server overloaded.\n";

bool isListenAnyBindAddress(const std::string& address) {
    return address.empty() || address == "0.0.0.0";
}

boost::asio::ip::tcp::endpoint resolveBindEndpoint(boost::asio::io_context& io_ctx,
                                                   const std::string& bindAddress,
                                                   unsigned short port,
                                                   boost::system::error_code& ec) {
    using boost::asio::ip::tcp;

    if (isListenAnyBindAddress(bindAddress)) {
        ec.clear();
        return tcp::endpoint(tcp::v4(), port);
    }
    if (bindAddress == "::" || bindAddress == "[::]") {
        ec.clear();
        return tcp::endpoint(tcp::v6(), port);
    }

    boost::asio::ip::address addr = boost::asio::ip::make_address(bindAddress, ec);
    if (!ec) {
        return tcp::endpoint(addr, port);
    }

    ec.clear();
    tcp::resolver resolver(io_ctx);
    const auto results = resolver.resolve(bindAddress, std::to_string(port), ec);
    if (ec || results.empty()) {
        return {};
    }
    for (const auto& entry : results) {
        if (entry.endpoint().address().is_v4()) {
            return tcp::endpoint(entry.endpoint().address(), port);
        }
    }
    return tcp::endpoint(results.begin()->endpoint().address(), port);
}
}  // namespace

void Geruest::statusPersistenceLoop() {
    while (running.load(std::memory_order_relaxed)) {
        for (int i = 0; i < 36000 && running.load(std::memory_order_relaxed); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!running.load(std::memory_order_relaxed)) {
            break;
        }
        if (!serverData.savePersistentMetricsToFile(_statusPersistencePath)) {
            sendToLoggerError("Status metrics persistence: periodic save failed for " + _statusPersistencePath);
        }
    }
}

int Geruest::getListenPort() const {
    if (!acceptor_.has_value() || !acceptor_->is_open()) {
        return -1;
    }
    boost::system::error_code ec;
    const auto                ep = acceptor_->local_endpoint(ec);
    if (ec) {
        return -1;
    }
    return static_cast<int>(ep.port());
}

bool Geruest::init() {
    serverData.ensureCacheBustToken();
    if (!serverData.getCacheBustToken().empty()) {
        sendToLogger("Cache bust token: " + serverData.getCacheBustToken());
    }

    boost::system::error_code ec;

    const auto bind_ep =
        resolveBindEndpoint(io_ctx_, bindAddress_, static_cast<unsigned short>(port), ec);
    if (ec) {
        sendToLoggerError("Bind address resolution failed for BIND_ADDRESS '" + bindAddress_ +
                          "': " + ec.message());
        return false;
    }

    acceptor_.emplace(io_ctx_);
    acceptor_->open(bind_ep.protocol(), ec);
    if (ec) {
        sendToLoggerError("Acceptor open failed for " + bind_ep.address().to_string() + ":" +
                          std::to_string(bind_ep.port()) + ": " + ec.message());
        acceptor_.reset();
        return false;
    }

    acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
    acceptor_->bind(bind_ep, ec);
    if (ec) {
        sendToLoggerError("Bind failed on " + bind_ep.address().to_string() + ":" +
                          std::to_string(bind_ep.port()) + ": " + ec.message());
        acceptor_.reset();
        return false;
    }

    acceptor_->listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        sendToLoggerError("Listen failed on " + bind_ep.address().to_string() + ":" +
                          std::to_string(bind_ep.port()) + ": " + ec.message());
        acceptor_.reset();
        return false;
    }

    const int boundPort = getListenPort();
    boost::system::error_code ep_ec;
    const auto local = acceptor_->local_endpoint(ep_ec);
    if (!ep_ec) {
        sendToLogger("Server listening on " + local.address().to_string() + ":" +
                     std::to_string(boundPort >= 0 ? boundPort : port));
    } else {
        sendToLogger("Server started on port " + std::to_string(boundPort >= 0 ? boundPort : port));
    }
    return true;
}

void Geruest::doAccept() {
    if (!running.load(std::memory_order_relaxed) || !acceptor_.has_value() || !acceptor_->is_open()) {
        return;
    }

    acceptor_->async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
        if (!running.load(std::memory_order_relaxed)) {
            return;
        }
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                serverData.recordAcceptError();
                const bool fdExhausted =
                    (ec == boost::asio::error::no_descriptors) ||
                    (ec == boost::system::errc::too_many_files_open) ||
                    (ec == boost::system::errc::too_many_files_open_in_system);
                if (fdExhausted) {
                    serverData.recordAcceptEmfile();
                }
                sendToLoggerError("Accept failed: " + ec.message());
                scheduleAcceptRetry(fdExhausted ? std::chrono::milliseconds(50)
                                                : std::chrono::milliseconds(5));
            }
            return;
        }

        std::string ip = "unknown";
        try {
            ip = socket.remote_endpoint().address().to_string();
        } catch (...) {}

        const size_t cap  = _maxQueueSize;
        const size_t prev = _activeSessions.fetch_add(1U, std::memory_order_acq_rel);
        if (prev >= cap) {
            _activeSessions.fetch_sub(1U, std::memory_order_acq_rel);
            serverData.recordQueueRejection();
            serverData.recordQueueFill(100.0f);
            serverData.recordOverloadHttpResponse();
            boost::system::error_code write_ec;
            boost::asio::write(socket, boost::asio::buffer(kOverloadedResponse, std::strlen(kOverloadedResponse)), write_ec);
            boost::system::error_code close_ec;
            socket.close(close_ec);
            if (running.load(std::memory_order_relaxed)) {
                doAccept();
            }
            return;
        }

        if (_maxQueueSize > 0) {
            const float fill =
                100.0f * static_cast<float>(prev + 1U) / static_cast<float>(_maxQueueSize);
            serverData.recordQueueFill(fill > 100.0f ? 100.0f : (fill < 0.0f ? 0.0f : fill));
        }

        std::make_shared<HttpSession>(*this, std::move(socket), std::move(ip))->start();

        if (running.load(std::memory_order_relaxed)) {
            doAccept();
        }
    });
}

void Geruest::scheduleAcceptRetry(std::chrono::milliseconds delay) {
    if (!running.load(std::memory_order_relaxed) || !acceptor_.has_value() || !acceptor_->is_open()) {
        return;
    }

    if (!_acceptRetryTimer.has_value()) {
        _acceptRetryTimer.emplace(io_ctx_);
    }
    _acceptRetryTimer->expires_after(delay);
    _acceptRetryTimer->async_wait([this](const boost::system::error_code& timer_ec) {
        if (timer_ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (running.load(std::memory_order_relaxed) && acceptor_.has_value() && acceptor_->is_open()) {
            doAccept();
        }
    });
}

void Geruest::releaseSessionSlot() {
    const size_t prev = _activeSessions.fetch_sub(1U, std::memory_order_acq_rel);
    const size_t now  = prev > 0U ? prev - 1U : 0U;
    if (_maxQueueSize > 0) {
        const float fill = 100.0f * static_cast<float>(now) / static_cast<float>(_maxQueueSize);
        serverData.recordQueueFill(fill > 100.0f ? 100.0f : (fill < 0.0f ? 0.0f : fill));
    }
}

void Geruest::start() {
    if (!serverData.loadPersistentMetricsFromFile(_statusPersistencePath)) {
        sendToLoggerError("Status metrics persistence: invalid or unsupported file " + _statusPersistencePath);
    }

    running.store(true, std::memory_order_relaxed);
    startWorkers();
    try {
        _statusPersistenceThread = std::thread(&Geruest::statusPersistenceLoop, this);
    } catch (...) {
        running.store(false, std::memory_order_relaxed);
        stopWorkers();
        throw;
    }

    sendToLogger("Waiting for connections...");
    sendToLogger("Worker threads: " + std::to_string(_workerThreadCount) +
                 ", Max concurrent sessions: " + std::to_string(_maxQueueSize));

    io_ctx_.run();

    stopWorkers();
    if (_statusPersistenceThread.joinable()) {
        _statusPersistenceThread.join();
    }
    if (!serverData.savePersistentMetricsToFile(_statusPersistencePath)) {
        sendToLoggerError("Status metrics persistence: shutdown save failed for " + _statusPersistencePath);
    }
    sendToLogger("Server stopped.");
}

void Geruest::stop() {
    sendToLogger("Stopping server at next opportunity.");
    running.store(false, std::memory_order_relaxed);

    boost::system::error_code ec;
    if (acceptor_.has_value() && acceptor_->is_open()) {
        acceptor_->close(ec);
    }
    if (_acceptRetryTimer.has_value()) {
        _acceptRetryTimer->cancel();
    }
    work_guard_.reset();
    io_ctx_.stop();
}

bool Geruest::isRunning() { return running.load(std::memory_order_relaxed); }

}  // namespace geruest
