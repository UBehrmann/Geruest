/**
 * @file GzipResponse.cpp
 */

#include "GzipResponse.hpp"

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"

#include <zlib.h>

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace geruest {
namespace {

constexpr size_t kMinCompressBytes = 1024;

bool iequalsAscii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool acceptsGzip(std::string_view acceptEncoding) {
    while (!acceptEncoding.empty()) {
        const size_t comma = acceptEncoding.find(',');
        const std::string_view token = acceptEncoding.substr(0, comma);
        size_t start = 0;
        while (start < token.size() && std::isspace(static_cast<unsigned char>(token[start]))) {
            ++start;
        }
        size_t end = token.size();
        while (end > start && std::isspace(static_cast<unsigned char>(token[end - 1]))) {
            --end;
        }
        const std::string_view trimmed = token.substr(start, end - start);
        const size_t semi = trimmed.find(';');
        const std::string_view coding = trimmed.substr(0, semi);
        if (iequalsAscii(coding, "gzip")) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        acceptEncoding.remove_prefix(comma + 1);
    }
    return false;
}

bool isCompressibleContentType(std::string_view contentType) {
    if (contentType.empty()) {
        return false;
    }
    const size_t semi = contentType.find(';');
    const std::string_view base = contentType.substr(0, semi);
    if (base.size() >= 5 && iequalsAscii(base.substr(0, 5), "text/")) {
        return true;
    }
    return iequalsAscii(base, "application/json") || iequalsAscii(base, "application/javascript") ||
           iequalsAscii(base, "application/xml") || iequalsAscii(base, "text/xml");
}

std::optional<std::string> gzipCompress(std::string_view input) {
    if (input.empty()) {
        return std::string();
    }

    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return std::nullopt;
    }

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());

    std::string out;
    out.resize(deflateBound(&stream, static_cast<uLong>(input.size())));

    int ret = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(out.data() + stream.total_out);
        stream.avail_out = static_cast<uInt>(out.size() - stream.total_out);
        ret = deflate(&stream, Z_FINISH);
        if (ret == Z_OK && stream.avail_out == 0) {
            out.resize(out.size() * 2);
        }
    } while (ret == Z_OK);

    const bool ok = ret == Z_STREAM_END;
    deflateEnd(&stream);
    if (!ok) {
        return std::nullopt;
    }

    out.resize(stream.total_out);
    return out;
}

}  // namespace

void applyResponseCompression(HTTPResponse& response, const HTTPRequest* request) {
    if (request == nullptr) {
        return;
    }
    if (response.getBody().size() < kMinCompressBytes) {
        return;
    }
    if (response.hasHeader("Content-Encoding")) {
        return;
    }
    if (!acceptsGzip(request->getHeaderView("accept-encoding"))) {
        return;
    }
    if (!isCompressibleContentType(response.getHeaderValue("Content-Type"))) {
        return;
    }

    const auto compressed = gzipCompress(response.getBody());
    if (!compressed.has_value() || compressed->size() >= response.getBody().size()) {
        return;
    }

    response.setBody(*compressed);
    response.setHeader("Content-Encoding", "gzip");
    response.setHeader("Vary", "Accept-Encoding");
}

}  // namespace geruest
