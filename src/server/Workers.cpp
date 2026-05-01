/**
 * @file server/Workers.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief io_context thread pool (Boost.Asio).
 */

#include "../Geruest.hpp"

namespace geruest {

void Geruest::startWorkers() {
    _workersRunning.store(true, std::memory_order_relaxed);
    work_guard_.emplace(boost::asio::make_work_guard(io_ctx_.get_executor()));

    const size_t extra = _workerThreadCount > 1U ? _workerThreadCount - 1U : 0U;
    _workerThreads.reserve(extra);
    for (size_t i = 0; i < extra; ++i) {
        _workerThreads.emplace_back(&Geruest::workerRunLoop, this);
    }

    boost::asio::post(io_ctx_, [this] { doAccept(); });

    sendToLogger("Started " + std::to_string(_workerThreadCount) + " worker threads");
}

void Geruest::stopWorkers() {
    if (!_workersRunning.load(std::memory_order_relaxed) && _workerThreads.empty()) {
        return;
    }

    _workersRunning.store(false, std::memory_order_relaxed);

    boost::system::error_code ec;
    if (acceptor_.has_value() && acceptor_->is_open()) {
        acceptor_->close(ec);
    }
    work_guard_.reset();
    io_ctx_.stop();

    for (auto& worker : _workerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    _workerThreads.clear();

    io_ctx_.restart();

    sendToLogger("All worker threads stopped");
}

void Geruest::workerRunLoop() { io_ctx_.run(); }

}  // namespace geruest
