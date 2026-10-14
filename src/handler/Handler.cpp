/**
 * @file Handler.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief
 */

#include "Handler.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <exception>
#include <string>

#include "GateEvaluation.hpp"
#include "data/CorsConfig.hpp"
#include "data/HTTPResponse.hpp"
#include "modules/ModuleHooks.hpp"
#include "security/Security.hpp"

namespace geruest {

Handler::Handler(boost::asio::ip::tcp::socket& socket, std::string clientIP, const ServerData& serverDataRef)
    : clientSocket(socket),
      serverData(serverDataRef),
      IP(std::move(clientIP)),
      writer_(clientSocket),
      responseScratch_(writer_.scratch()),
      framing_(clientSocket,
               [this](const std::string& msg) { sendToLoggerError(msg); },
               [this](const std::string& msg) { sendToLogger(msg, LogLevel::Warning); }),
      fileResolver_(serverDataRef, [this](const std::string& msg) { sendToLoggerError(msg); }),
      routeDispatcher_(serverDataRef, fileResolver_) {}

Handler::~Handler() {
    setInFlight(false);
}

void Handler::recordErrorMetric() const {
    if (_countRequestInMetrics) {
        serverData.metrics().add(ServerMetrics::Counter::InternalErrors);
    }
}

void Handler::recordErrorLogEntry(int status, const HTTPRequest* request) const {
    if (status < 400 && _pendingErrors.empty()) {
        return;
    }
    ServerMetrics::ErrorEntry e;
    e.status = status;
    e.ip = IP;
    e.message = std::move(_pendingErrors);
    _pendingErrors.clear();
    if (request != nullptr) {
        e.request = request->getRawRequestLine();
        e.forwardedFor = request->getHeaderView("x-forwarded-for");
        e.host = request->getHeaderView("host");
        e.referer = request->getHeaderView("referer");
        e.userAgent = request->getHeaderView("user-agent");
    }
    serverData.metrics().recordError(std::move(e));
}

void Handler::setInFlight(bool inFlight) {
    if (_inFlight != inFlight) {
        _inFlight = inFlight;
        serverData.metrics().adjustActiveRequests(inFlight ? 1 : -1);
    }
}

boost::asio::awaitable<bool> Handler::sendSocketAsync(const char* bufferToSend, size_t size) {
    co_return co_await writer_.sendSocketAsync(bufferToSend, size);
}

boost::asio::awaitable<bool> Handler::enforcePageAccessAsync(const HTTPRequest& request,
                                                             const std::string& pagePath,
                                                             PageAccessDenyStyle denyStyle,
                                                             const std::optional<ResolvedPageGate>& resolvedGate) {
    const std::string canonPage = canonicalRequestPath(pagePath);

    if (serverData.getBasicAuth().requiresAuth(canonPage)) {
        if (!serverData.getBasicAuth().authenticate(canonPage, request.getHeader("authorization"))) {
            const std::string header = buildAuthHeader();
            if (!co_await sendSocketAsync(header.c_str(), header.size())) {
                sendToLoggerError("Failed to send auth header");
            }
            co_return false;
        }
    }

    const auto gate =
        resolvedGate.has_value() ? resolvedGate : serverData.findResolvedPageGate(pagePath);
    if (!gate.has_value()) {
        co_return true;
    }

    const auto allowed = co_await evaluateResolvedGateAsync(
        *gate, request, [this](const std::string& msg) { sendToLoggerError(msg); }, "page");
    if (!allowed.has_value()) {
        HTTPResponse err = responseInternalServerError(&request);
        err.serializeTo(responseScratch_);
        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send page gate error for: " + pagePath);
        }
        co_return false;
    }
    if (*allowed) {
        co_return true;
    }

    if (denyStyle == PageAccessDenyStyle::Forbidden) {
        HTTPResponse forbidden = responseForbidden(&request);
        forbidden.serializeTo(responseScratch_);
        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            sendToLoggerError("Failed to send merged asset gate denial for: " + pagePath);
        }
        co_return false;
    }

    HTTPResponse redirectResponse("302 Found");
    redirectResponse.setHeader("Location",
                               serverData.resolvePageGateRedirect(gate->redirectTo, request.getPathString()));
    redirectResponse.setBody("");
    redirectResponse.serializeTo(responseScratch_);
    if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
        sendToLoggerError("Failed to send page gate denial for: " + pagePath);
    }
    co_return false;
}

boost::asio::awaitable<std::optional<HTTPResponse>> Handler::checkRouteGateDenialAsync(
    const HTTPRequest& request) const {
    const auto gate = serverData.findResolvedRouteGate(request.getPathString());
    if (!gate.has_value()) {
        co_return std::nullopt;
    }

    const auto allowed = co_await evaluateResolvedGateAsync(
        *gate, request, [this](const std::string& msg) { sendToLoggerError(msg); }, "route");
    if (!allowed.has_value()) {
        co_return responseInternalServerError(&request);
    }
    if (*allowed) {
        co_return std::nullopt;
    }

    co_return responseForbidden(&request);
}

void Handler::sendToLogger(const std::string& message, LogLevel level) const {
    serverData.emitLog(level, message, IP);
}
void Handler::sendToLoggerPages(const std::string& message) const {
    serverData.emitLog(LogLevel::Info, "Page Log: " + message, IP);
}
void Handler::sendToLoggerAPI(const std::string& message) const {
    serverData.emitLog(LogLevel::Info, "API Log: " + message, IP);
}
void Handler::sendToLoggerUser(const std::string& message) const {
    serverData.emitLog(LogLevel::Info, "User Log: " + message, IP);
}
void Handler::sendToLoggerError(const std::string& message) const {
    serverData.emitLog(LogLevel::Error, message, IP);
    if (!_pendingErrors.empty()) {
        _pendingErrors += " | ";
    }
    _pendingErrors += message;
}

boost::asio::awaitable<void> Handler::runAsync() {
    const size_t maxRequestsPerConnection = serverData.getMaxRequestsPerConnection();
    const ServerMetrics& metrics = serverData.metrics();
    while (maxRequestsPerConnection == 0 || framing_.messageCount() < maxRequestsPerConnection) {
        const FramedRequest framed = co_await framing_.readNextRequestAsync(writer_, maxRequestsPerConnection);
        const ResponseWriter::Outcome framingOutcome = writer_.takeOutcome();
        if (framed.rejected) {
            metrics.add(ServerMetrics::Counter::FramingRejects);
            sendToLoggerError("Request rejected before routing (oversized headers, invalid Content-Length or body too large)");
        }
        recordErrorLogEntry(framingOutcome.status, nullptr);
        if (framed.outcome == FramingOutcome::Abort) {
            co_return;
        }
        if (framed.outcome != FramingOutcome::Request || !framed.messageBacking) {
            break;
        }

        HTTPRequest hTTPRequest(std::move(framed.messageBacking), IP, serverData.getRoot(),
                                serverData.getDatabaseClient());
        requestStream = std::istringstream();

        _countRequestInMetrics = !ServerMetrics::isMetricsExcludedPath(hTTPRequest.getPathString());
        if (_countRequestInMetrics) {
            metrics.add(ServerMetrics::Counter::Requests);
        }
        {
            const auto reqStart = std::chrono::steady_clock::now();
            setInFlight(true);
            co_await handleRequestAsync(&hTTPRequest);
            setInFlight(false);

            const ResponseWriter::Outcome outcome = writer_.takeOutcome();
            if (outcome.writeFailed) {
                metrics.add(ServerMetrics::Counter::WriteFailures);
            }
            if (_countRequestInMetrics) {
                metrics.recordStatus(outcome.status);
            }
            recordErrorLogEntry(outcome.status, &hTTPRequest);
            if (_upgraded) {
                co_return;
            }
            if (_countRequestInMetrics) {
                const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                           std::chrono::steady_clock::now() - reqStart)
                                           .count();
                metrics.recordLatency(elapsedUs <= 0 ? 0u
                                      : elapsedUs > 0xFFFFFFFFLL ? 0xFFFFFFFFu
                                                                 : static_cast<uint32_t>(elapsedUs));
            }
        }

        if (httpShouldCloseAfterResponse(hTTPRequest.getRawRequestLine(), hTTPRequest.getHeaderView("connection"))) {
            break;
        }
    }
    co_return;
}

boost::asio::awaitable<bool> Handler::tryHandleWebSocketAsync(HTTPRequest* request) {
    if (const auto& fn = modules::webSocketUpgradeFn()) {
        co_return co_await fn(*this, request);
    }
    co_return false;
}

boost::asio::awaitable<void> Handler::handleRequestAsync(HTTPRequest* request) {
    if (request == nullptr) {
        recordErrorMetric();
        sendToLoggerError("HTTPRequest is null.");
        std::string header = buildInternalServerErrorHeader();
        if (!co_await sendSocketAsync(header.c_str(), header.size())) {
            sendToLoggerError("Failed to send internal server error response");
        }
        co_return;
    }

    const CorsConfig& cors = serverData.getCorsConfig();
    if (request->getMethod() == "OPTIONS" && cors.isEnabled() && cors.matchesPath(request->getPathString())) {
        if (cors.resolveOrigin(request->getHeaderView("origin")).has_value()) {
            HTTPResponse preflight = responseNoContent(request);
            applyCorsHeaders(preflight, cors, request, true);
            preflight.serializeTo(responseScratch_);
            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                sendToLoggerError("Failed to send CORS preflight for: " + request->getPathString());
            }
        } else {
            HTTPResponse forbidden = responseForbidden(request);
            forbidden.serializeTo(responseScratch_);
            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                sendToLoggerError("Failed to send CORS preflight denial for: " + request->getPathString());
            }
        }
        co_return;
    }

    if (co_await tryHandleWebSocketAsync(request)) {
        co_return;
    }

    co_await routeDispatcher_.dispatchAsync(request, *this);
}

boost::asio::awaitable<void> Handler::sendResponseAsync(const std::string& status, const std::string& contentType,
                                                        const std::string& content) {
    co_await writer_.sendResponseAsync(status, contentType, content,
                                       [this](const std::string& msg) { sendToLoggerError(msg); });
    co_return;
}

boost::asio::awaitable<void> Handler::sendFileAsync(const std::string& contentType, const std::string& contentPath,
                                                    HTTPRequest* httpRequest) {
    co_await writer_.sendFileAsync(contentType, contentPath, httpRequest, *this);
    co_return;
}

}  // namespace geruest
