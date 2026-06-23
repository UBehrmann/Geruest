/**
 * @file ResponseWriter.cpp
 */

#include "ResponseWriter.hpp"

#include "Handler.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <atomic>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>
#ifdef __linux__
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "geruest/BuildConfig.hpp"
#include "modules/ModuleHooks.hpp"
#include "data/TextResponseCache.hpp"

namespace {

std::atomic<uint64_t> gSendfileHitCount{0};
std::atomic<uint64_t> gSendfileFallbackCount{0};

}  // namespace

namespace geruest {

ResponseWriter::ResponseWriter(boost::asio::ip::tcp::socket& socket) : socket_(socket) {}

boost::asio::awaitable<bool> ResponseWriter::sendSocketAsync(const char* bufferToSend, size_t size) {
    size_t startPos = 0;
    while (startPos < size) {
        const size_t chunkSize = std::min(HttpFraming::kBufferSize, size - startPos);
        try {
            co_await boost::asio::async_write(socket_, boost::asio::buffer(bufferToSend + startPos, chunkSize),
                                              boost::asio::use_awaitable);
        } catch (const boost::system::system_error&) {
            co_return false;
        }
        startPos += chunkSize;
    }
    co_return true;
}

boost::asio::awaitable<bool> ResponseWriter::sendFileBodyZeroCopyAsync(const std::string& contentPath,
                                                                       size_t fileSize) {
#ifndef __linux__
    (void)contentPath;
    (void)fileSize;
    co_return false;
#else
    const int fileFd = ::open(contentPath.c_str(), O_RDONLY);
    if (fileFd < 0) {
        co_return false;
    }

    struct stat st {};
    if (::fstat(fileFd, &st) != 0 || !S_ISREG(st.st_mode)) {
        ::close(fileFd);
        co_return false;
    }

    const int socketFd = socket_.native_handle();
    off_t   offset = 0;
    size_t  remaining = fileSize;

    while (remaining > 0) {
        const size_t toSend = std::min(remaining, static_cast<size_t>(SSIZE_MAX));
        const ssize_t sent = ::sendfile(socketFd, fileFd, &offset, toSend);

        if (sent > 0) {
            remaining -= static_cast<size_t>(sent);
            continue;
        }

        if (sent == 0) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            try {
                co_await socket_.async_wait(boost::asio::ip::tcp::socket::wait_write, boost::asio::use_awaitable);
                continue;
            } catch (const boost::system::system_error&) {
                ::close(fileFd);
                co_return false;
            }
        }

        ::close(fileFd);
        co_return false;
    }

    ::close(fileFd);
    co_return remaining == 0;
#endif
}

boost::asio::awaitable<void> ResponseWriter::sendResponseAsync(const std::string& status,
                                                               const std::string& contentType,
                                                               const std::string& content,
                                                               const std::function<void(const std::string&)>& logError) {
    HTTPResponse hdr(status);
    hdr.setHeader("Content-Type", contentType);
    hdr.setHeader("Content-Length", std::to_string(content.size()));
    hdr.serializeTo(responseScratch_);
    responseScratch_ += content;

    if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
        logError("Failed to send response: " + status);
    }
    co_return;
}

boost::asio::awaitable<void> ResponseWriter::sendNotFoundResponseAsync(HTTPRequest* httpRequest, Handler& host) {
    host.record4xxMetric();
    if (httpRequest != nullptr) {
        host.sendToLoggerError("404 route miss. path=" + httpRequest->getPathString() +
                               " request_line=" + httpRequest->getRawRequestLine());
    } else {
        host.sendToLoggerError("404 route miss. null request context");
    }
    if (host.serverData.hasNotFoundPage() && httpRequest != nullptr) {
        std::string notFoundPath = host.serverData.getNotFoundPage();
        if (!notFoundPath.empty() && notFoundPath[0] != '/') {
            notFoundPath = "/" + notFoundPath;
        }

        if (host.serverData.hasLanguages()) {
            notFoundPath = host.serverData.localizePathWithRequestLanguage(notFoundPath, httpRequest->getPathString());
        }

        const std::string extension = StaticFileResolver::getExtension(notFoundPath);
        const std::string contentType = StaticFileResolver::getContentType(extension);
        std::string       resolvedNotFoundPath = notFoundPath;

        if (!(std::filesystem::path(resolvedNotFoundPath).is_absolute() &&
              std::filesystem::exists(resolvedNotFoundPath))) {
            resolvedNotFoundPath = host.fileResolver_.buildPath(notFoundPath, extension, *httpRequest);
        }

        if (!resolvedNotFoundPath.empty()) {
            if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
                const std::optional<std::string> body =
                    modules::processTextContent(contentType, resolvedNotFoundPath, host.serverData);

                if (body.has_value() && !body->empty()) {
                    co_await sendResponseAsync("404 Not Found", contentType, *body,
                                               [ &host ](const std::string& msg) { host.sendToLoggerError(msg); });
                    co_return;
                }
            } else {
                std::ifstream file(resolvedNotFoundPath, std::ios::binary);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    co_await sendResponseAsync("404 Not Found", contentType, content,
                                               [ &host ](const std::string& msg) { host.sendToLoggerError(msg); });
                    co_return;
                }
            }
        }

        host.sendToLoggerError("Configured 404 page could not be loaded: " + host.serverData.getNotFoundPage());
    }

    co_await sendResponseAsync("404 Not Found", "text/html", "<html><body><h1>404 Not Found</h1></body></html>",
                               [ &host ](const std::string& msg) { host.sendToLoggerError(msg); });
    co_return;
}

boost::asio::awaitable<void> ResponseWriter::sendServiceUnavailableResponseAsync(const std::string& why,
                                                                                 Handler& host) {
    host.record5xxMetric();
    host.serverData.recordFileOpenFailure();
    host.sendToLoggerError("Service unavailable while serving file: " + why);
    co_await sendResponseAsync("503 Service Unavailable", "text/plain",
                               "Service temporarily unavailable. Please retry.\n",
                               [ &host ](const std::string& msg) { host.sendToLoggerError(msg); });
    co_return;
}

boost::asio::awaitable<void> ResponseWriter::sendFileAsync(const std::string& contentType,
                                                           const std::string& contentPath, HTTPRequest* httpRequest,
                                                           Handler& host) {
    char bufferToSend[HttpFraming::kBufferSize];

    if (contentType == "text/html" || contentType == "text/javascript" || contentType == "text/css") {
        const std::string cacheKey = TextResponseCache::makeKey(contentType, contentPath);
        const std::string requestPath = httpRequest != nullptr ? httpRequest->getPathString() : std::string();
        std::optional<std::string> mergedAssetOwnerPage;
        if (httpRequest != nullptr && (contentType == "text/javascript" || contentType == "text/css")) {
            mergedAssetOwnerPage = host.serverData.findMergedAssetOwnerPagePath(requestPath);
        }

        const std::optional<ResolvedPageGate> resolvedPageGate =
            contentType == "text/html" && httpRequest != nullptr
                ? host.serverData.findResolvedPageGate(requestPath)
                : std::nullopt;
        const bool perRequestText =
            httpRequest != nullptr &&
            ((contentType == "text/html" &&
              (host.serverData.getBasicAuth().requiresAuth(requestPath) || resolvedPageGate.has_value())) ||
             (mergedAssetOwnerPage.has_value() && host.serverData.pageRequiresAccessControl(*mergedAssetOwnerPage)));

        if (perRequestText && httpRequest) {
            const std::string& accessPath = contentType == "text/html" ? requestPath : *mergedAssetOwnerPage;
            const PageAccessDenyStyle denyStyle =
                contentType == "text/html" ? PageAccessDenyStyle::Redirect : PageAccessDenyStyle::Forbidden;
            if (!co_await host.enforcePageAccessAsync(*httpRequest, accessPath, denyStyle, resolvedPageGate)) {
                co_return;
            }
        }

        if (!perRequestText) {
            if (const std::shared_ptr<const std::string> cached = host.serverData.textResponseCache().lookup(
                    cacheKey, contentPath, host.serverData.isDevMode(), host.serverData.getTextResponseCacheMaxEntryBytes(),
                    host.serverData.getTextResponseCacheMaxTotalBytes())) {
                if (!co_await sendSocketAsync(cached->data(), cached->size())) {
                    host.sendToLoggerError("Failed to send cached file: " + contentPath);
                }
                co_return;
            }
        }

        const std::optional<std::string> processed =
            modules::processTextContent(contentType, contentPath, host.serverData);

        if (!processed.has_value()) {
            co_await sendNotFoundResponseAsync(httpRequest, host);
            co_return;
        }

        if (processed->empty()) {
            if (!contentPath.empty() && std::filesystem::exists(contentPath)) {
                co_await sendServiceUnavailableResponseAsync(contentPath, host);
            } else {
                co_await sendNotFoundResponseAsync(httpRequest, host);
            }
            co_return;
        }

        HTTPResponse htmlResponse("200 OK");
        htmlResponse.setHeader("Content-Type", contentType);
        htmlResponse.setBody(*processed);
        htmlResponse.serializeTo(responseScratch_);
        if (!perRequestText) {
            host.serverData.textResponseCache().store(cacheKey, contentPath, responseScratch_, host.serverData.isDevMode(),
                                                      host.serverData.getTextResponseCacheMaxEntryBytes(),
                                                      host.serverData.getTextResponseCacheMaxTotalBytes());
        }

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            host.sendToLoggerError("Failed to send file: " + contentPath);
        }

    } else {
#if GERUEST_ENABLE_ASSETS
        if (contentType == "image/webp" && host.serverData.isDevMode() && host.serverData.getWebPConversion()) {
            auto cachedWebP = host.serverData.devAssetCache().getWebP(contentPath);
            if (cachedWebP && !cachedWebP->empty()) {
                HTTPResponse htmlResponse("200 OK");
                htmlResponse.setHeader("Content-Type", contentType);
                htmlResponse.setHeader("Content-Length", std::to_string(cachedWebP->size()));
                htmlResponse.serializeTo(responseScratch_);

                if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                    host.sendToLoggerError("Failed to send cached WebP header: " + contentPath);
                    co_return;
                }

                if (!co_await sendSocketAsync(reinterpret_cast<const char*>(cachedWebP->data()), cachedWebP->size())) {
                    host.sendToLoggerError("Failed to send cached WebP data: " + contentPath);
                }
                co_return;
            }
        }
#endif

        std::error_code fsErr;
        const uintmax_t fileSizeRaw = std::filesystem::file_size(contentPath, fsErr);
        if (!fsErr && fileSizeRaw <= static_cast<uintmax_t>(SIZE_MAX)) {
            const size_t fileSize = static_cast<size_t>(fileSizeRaw);

            HTTPResponse htmlResponse("200 OK");
            htmlResponse.setHeader("Content-Type", contentType);
            htmlResponse.setHeader("Content-Length", std::to_string(fileSize));
            htmlResponse.serializeTo(responseScratch_);

            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                host.sendToLoggerError("Failed to send binary file header: " + contentPath);
                co_return;
            }

            if (co_await sendFileBodyZeroCopyAsync(contentPath, fileSize)) {
                ++gSendfileHitCount;
                co_return;
            }

            ++gSendfileFallbackCount;
            host.sendToLoggerError("Zero-copy sendfile failed after header write: " + contentPath);
            co_return;
        }

        std::ifstream file(contentPath, std::ios::binary);

        if (!file.is_open()) {
            if (contentType == "image/webp" && host.serverData.getWebPConversion()) {
                std::string basePath = contentPath;
                size_t      dotPos = basePath.find_last_of('.');
                if (dotPos != std::string::npos) {
                    basePath = basePath.substr(0, dotPos);
                }

                std::string              sourcePath;
                std::vector<std::string> extensions = {".jpg", ".jpeg", ".png"};

                for (const auto& ext : extensions) {
                    if (std::filesystem::exists(basePath + ext)) {
                        sourcePath = basePath + ext;
                        break;
                    }
                }

                if (!sourcePath.empty()) {
                    bool cacheOnly = host.serverData.isDevMode();
                    if (modules::convertWebpImage(sourcePath, contentPath, cacheOnly,
                                                  host.serverData.getWebPQuality())) {
                        if (cacheOnly) {
                            auto webpData = host.serverData.devAssetCache().getWebP(contentPath);
                            if (webpData && !webpData->empty()) {
                                HTTPResponse webpResponse("200 OK");
                                webpResponse.setHeader("Content-Type", contentType);
                                webpResponse.setHeader("Content-Length", std::to_string(webpData->size()));
                                webpResponse.serializeTo(responseScratch_);

                                if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                                    host.sendToLoggerError("Failed to send on-demand WebP header: " + contentPath);
                                    co_return;
                                }

                                if (!co_await sendSocketAsync(reinterpret_cast<const char*>(webpData->data()),
                                                              webpData->size())) {
                                    host.sendToLoggerError("Failed to send on-demand WebP data: " + contentPath);
                                }
                                co_return;
                            }
                        } else {
                            file.open(contentPath, std::ios::binary);
                        }
                    } else {
                        host.sendToLoggerError("WebP conversion failed for " + sourcePath +
                                               ", serving original format instead");
                        std::ifstream origFile(sourcePath, std::ios::binary);
                        if (origFile.is_open()) {
                            origFile.seekg(0, std::ios::end);
                            size_t origSize = static_cast<size_t>(origFile.tellg());
                            origFile.seekg(0, std::ios::beg);

                            const std::string origType =
                                StaticFileResolver::getContentType(StaticFileResolver::getExtension(sourcePath));
                            HTTPResponse origResp("200 OK");
                            origResp.setHeader("Content-Type", origType);
                            origResp.setHeader("Content-Length", std::to_string(origSize));
                            origResp.serializeTo(responseScratch_);

                            if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
                                host.sendToLoggerError("Failed to send fallback image header: " + sourcePath);
                                co_return;
                            }
                            char fallbackBuf[HttpFraming::kBufferSize];
                            while (!origFile.eof()) {
                                origFile.read(fallbackBuf, HttpFraming::kBufferSize);
                                if (!co_await sendSocketAsync(fallbackBuf, static_cast<size_t>(origFile.gcount()))) {
                                    host.sendToLoggerError("Failed to send fallback image data: " + sourcePath);
                                    break;
                                }
                            }
                        } else if (std::filesystem::exists(sourcePath)) {
                            co_await sendServiceUnavailableResponseAsync(sourcePath, host);
                        }
                        co_return;
                    }
                }
            }

            if (!file.is_open()) {
                if (!contentPath.empty() && std::filesystem::exists(contentPath)) {
                    co_await sendServiceUnavailableResponseAsync(contentPath, host);
                } else {
                    host.sendToLogger("File not found: " + contentPath);
                    co_await sendNotFoundResponseAsync(httpRequest, host);
                }
                co_return;
            }
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        HTTPResponse htmlResponse("200 OK");
        htmlResponse.setHeader("Content-Type", contentType);
        htmlResponse.setHeader("Content-Length", std::to_string(fileSize));
        htmlResponse.serializeTo(responseScratch_);

        if (!co_await sendSocketAsync(responseScratch_.data(), responseScratch_.size())) {
            host.sendToLoggerError("Failed to send binary file header: " + contentPath);
            file.close();
            co_return;
        }

        while (!file.eof()) {
            file.read(bufferToSend, HttpFraming::kBufferSize);
            if (!co_await sendSocketAsync(bufferToSend, static_cast<size_t>(file.gcount()))) {
                host.sendToLoggerError("Failed to send binary file chunk: " + contentPath);
                break;
            }
        }

        file.close();
    }
    co_return;
}

}  // namespace geruest
