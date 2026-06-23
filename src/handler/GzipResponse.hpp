/**
 * @file GzipResponse.hpp
 * @brief Optional gzip Content-Encoding for HTTP responses.
 */

#ifndef GERUEST_GZIPRESPONSE_HPP
#define GERUEST_GZIPRESPONSE_HPP

namespace geruest {

class HTTPRequest;
class HTTPResponse;

/** Compress response body when client accepts gzip and payload is large enough. */
void applyResponseCompression(HTTPResponse& response, const HTTPRequest* request);

}  // namespace geruest

#endif  // GERUEST_GZIPRESPONSE_HPP
