/**
 * @file StaticFileResolver.cpp
 */

#include "StaticFileResolver.hpp"

#include <unordered_map>

#include "security/Security.hpp"

namespace {

const std::unordered_map<std::string, std::string>& assetRootByExtension() {
    static const std::unordered_map<std::string, std::string> m = {
        {"css", "/assets/css"},
        {"js", "/assets/js"},
        {"jpg", "/assets/images"},
        {"jpeg", "/assets/images"},
        {"png", "/assets/images"},
        {"gif", "/assets/images"},
        {"svg", "/assets/images"},
        {"ico", "/assets/images"},
        {"webp", "/assets/images"},
        {"JSON", "/assets/JSONs"},
        {"pdf", "/assets/docs"},
        {"zip", "/assets/docs"},
        {"mp3", "/assets/audio"},
        {"mp4", "/assets/video"},
        {"xml", "/assets/docs"},
        {"csv", "/assets/docs"},
        {"txt", "/assets/docs"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& contentTypeByExtension() {
    static const std::unordered_map<std::string, std::string> m = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "text/javascript"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"JSON", "application/JSON"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"xml", "application/xml"},
        {"csv", "text/csv"},
        {"txt", "text/plain"},
    };
    return m;
}

}  // namespace

namespace geruest {

StaticFileResolver::StaticFileResolver(const ServerData& serverData,
                                         std::function<void(const std::string&)> logError)
    : serverData_(serverData), logError_(std::move(logError)) {}

std::string StaticFileResolver::getExtension(const std::string& path) {
    if (path.find('.') == std::string::npos) {
        return "html";
    }
    return path.substr(path.find('.') + 1);
}

std::string StaticFileResolver::getContentType(const std::string& extension) {
    const auto& types = contentTypeByExtension();
    const auto  it    = types.find(extension);
    return it != types.end() ? it->second : "application/octet-stream";
}

std::string StaticFileResolver::buildPath(std::string& pathReceived, const std::string& extension,
                                          const HTTPRequest& request) const {
    if (!Security::isSafePath(serverData_.getRoot(), pathReceived)) {
        if (logError_) {
            logError_("Path traversal attempt blocked: " + pathReceived);
        }
        return "";
    }

    std::string htmlMount = "/html";

    const std::string_view language = request.getHeaderView("accept-language");

    if (extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "gif" ||
        extension == "svg" || extension == "ico" || extension == "webp") {
        if (pathReceived.find("/assets/") == 0) {
            return serverData_.getRoot() + pathReceived;
        }

        const std::string_view refererSv = request.getHeaderView("referer");
        if (!refererSv.empty()) {
            std::string referer(refererSv);

            const size_t lastSlashInReferer = referer.find_last_of('/');
            if (lastSlashInReferer != std::string::npos) {
                const std::string refererBase = referer.substr(0, lastSlashInReferer + 1);

                std::string host = "localhost";
                const std::string_view hostSv = request.getHeaderView("host");
                if (!hostSv.empty()) {
                    host.assign(hostSv);
                }
                std::string scheme = "http";
                const std::string_view forwardedProto = request.getHeaderView("x-forwarded-proto");
                if (!forwardedProto.empty()) {
                    scheme.assign(forwardedProto);
                    for (char& ch : scheme) {
                        if (ch >= 'A' && ch <= 'Z') {
                            ch = static_cast<char>(ch + ('a' - 'A'));
                        }
                    }
                    if (scheme != "http" && scheme != "https") {
                        scheme = "http";
                    }
                }
                const std::string fullRequestUrl = scheme + "://" + host + pathReceived;

                if (fullRequestUrl.find(refererBase) == 0) {
                    const std::string relativePath = fullRequestUrl.substr(refererBase.length());
                    pathReceived = "/" + relativePath;
                }
            }
        } else {
            const size_t lastSlash = pathReceived.find_last_of('/');
            pathReceived = (lastSlash != std::string::npos) ? pathReceived.substr(lastSlash) : "/" + pathReceived;
        }
    } else if (!serverData_.languagePrefixFromPath(pathReceived).has_value()) {
        if (pathReceived.size() == 1) {
            if (serverData_.hasLanguages()) {
                const std::string preferredLang = serverData_.resolvePreferredLanguage(language);
                return serverData_.getRoot() + "/html/" + preferredLang + "/index.html";
            }

            return serverData_.getRoot() + "/html/index.html";
        }

        if (serverData_.hasLanguages()) {
            const std::string preferredLang = serverData_.resolvePreferredLanguage(language);
            htmlMount = "/html/" + preferredLang;
        } else {
            htmlMount = "/html";
        }
    } else if (pathReceived.size() == 4) {
        pathReceived += "/index";
    } else if (extension != "html" && extension != "htm") {
        pathReceived = pathReceived.substr(3);
    }

    if (pathReceived.find('.') == std::string::npos) {
        pathReceived += ".html";
    }

    std::string mount;
    if (extension == "html" || extension == "htm") {
        mount = htmlMount;
    } else {
        const auto& roots = assetRootByExtension();
        const auto  it    = roots.find(extension);
        if (it == roots.end()) {
            return "";
        }
        mount = it->second;
    }

    const std::string finalPath = serverData_.getRoot() + mount + pathReceived;
    if (!Security::isSafePath(serverData_.getRoot(), mount + pathReceived)) {
        if (logError_) {
            logError_("Path traversal attempt blocked after assembly: " + finalPath);
        }
        return "";
    }
    return finalPath;
}

}  // namespace geruest
