/**
 * @file server/HttpSession.cpp
 */

#include "HttpSession.hpp"

#include "../Geruest.hpp"
#include "../handler/Handler.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace geruest {

HttpSession::HttpSession(Geruest& server, tcp_socket&& socket, std::string clientIp)
    : server_(server)
    , strand_(boost::asio::make_strand(socket.get_executor()))
    , socket_(strand_)
    , clientIp_(std::move(clientIp)) {
    boost::system::error_code ec;
    socket_.assign(boost::asio::ip::tcp::v4(), socket.release(), ec);
}

void HttpSession::start() {
    boost::asio::co_spawn(
        strand_,
        [self = shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->runCoro(); },
        boost::asio::detached);
}

boost::asio::awaitable<void> HttpSession::runCoro() {
    struct SlotRelease {
        Geruest* s;
        ~SlotRelease() {
            if (s != nullptr) {
                s->releaseSessionSlot();
            }
        }
    } release{&server_};

    try {
        Handler handler(socket_, clientIp_, server_.serverData);
        co_await handler.runAsync();
    } catch (const std::exception& e) {
        server_.serverData.recordError();
        server_.sendToLoggerError(std::string("Handler error: ") + e.what());
    } catch (...) {
        server_.serverData.recordError();
        server_.sendToLoggerError("Handler encountered an unknown error");
    }
    co_return;
}

}  // namespace geruest
