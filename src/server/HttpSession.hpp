/**
 * @file server/HttpSession.hpp
 * @brief Per-connection coroutine entry (Boost.Asio); runs Handler async loop on a strand.
 */

#ifndef GERUEST_HTTPSESSION_HPP
#define GERUEST_HTTPSESSION_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <string>

namespace geruest {

class Geruest;

/**
 * Owns the accepted client socket and spawns a strand-bound coroutine that runs Handler::runAsync().
 */
class HttpSession : public std::enable_shared_from_this<HttpSession> {
   public:
    using tcp_socket = boost::asio::ip::tcp::socket;

    HttpSession(Geruest& server, tcp_socket&& socket, std::string clientIp);

    void start();

   private:
    boost::asio::awaitable<void> runCoro();

    Geruest& server_;

    /** Serializes all async I/O for this connection across io_context worker threads. */
    boost::asio::strand<tcp_socket::executor_type> strand_;

    tcp_socket  socket_;
    std::string clientIp_;
};

}  // namespace geruest

#endif  // GERUEST_HTTPSESSION_HPP
