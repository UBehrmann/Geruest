/**
 * @file HTTPResponse.hpp
 * @date 12.07.24
 *
 * @author Urs Behrmann
 *
 * @brief This function is used to build HTTP headers.
 */

#ifndef GERUEST_HTTPRESPONSE_HPP
#define GERUEST_HTTPRESPONSE_HPP

#include <map>
#include <string>
#include <string_view>
#include <sstream>
#include "HTTPRequest.hpp"

namespace geruest {

/**
 * @class HTTPResponse
 * @brief A class for building dynamic HTTP responses with customizable headers.
 */
class HTTPResponse {
private:
    std::string status;
    std::multimap<std::string, std::string> headers; // Changed to multimap to allow duplicate keys
    std::string body;

public:
    /**
     * Constructor to initialize the response with a status code.
     * @param statusCode The HTTP status code (e.g., "200 OK").
     */
    explicit HTTPResponse(const std::string& statusCode);

    /**
     * Sets a custom header field.
     * @param key The header name.
     * @param value The header value.
     */
    void setHeader(const std::string& key, const std::string& value);

    /**
     * Adds a header that can coexist with other headers of the same key.
     * This allows multiple headers with the same name (e.g., multiple Set-Cookie headers).
     * @param key The header name.
     * @param value The header value to add.
     */
    void addHeader(const std::string& key, const std::string& value);

    /**
     * Sets the response body and updates the Content-Length header.
     * @param responseBody The response body.
     */
    void setBody(const std::string& responseBody);

    /**
     * Returns the HTTP status string (e.g., "200 OK").
     * @return The status string.
     */
    const std::string& getStatus() const { return status; }

    /**
     * Builds and returns the full HTTP response as a string.
     * @return The complete HTTP response.
     */
    std::string toString() const;
};

/**
 * Static functions that mirror existing inline functions
 */

[[maybe_unused]] std::string buildHeader(const std::string &status,
                               const std::string &contentType,
                               const std::string &size);

[[maybe_unused]] std::string buildBadRequestHeader();

[[maybe_unused]] std::string buildAuthHeader();

[[maybe_unused]] std::string buildForbiddenHeader();

[[maybe_unused]] std::string buildNotFoundHeader();

[[maybe_unused]] std::string buildMethodNotAllowedHeader();

[[maybe_unused]] std::string buildOKHeader();

[[maybe_unused]] std::string buildFailHeader();

[[maybe_unused]] std::string buildInternalServerErrorHeader();

// Inline functions that return HTTPResponse objects

// 200 OK
[[maybe_unused]] HTTPResponse responseOK(const HTTPRequest* request = nullptr);
// 201 Created
[[maybe_unused]] HTTPResponse responseCreated(const HTTPRequest* request = nullptr);
// 202 Accepted
[[maybe_unused]] HTTPResponse responseAccepted(const HTTPRequest* request = nullptr);
// 203 Non-Authoritative Information
[[maybe_unused]] HTTPResponse responseNonAuthoritative(const HTTPRequest* request = nullptr);
// 204 No Content
[[maybe_unused]] HTTPResponse responseNoContent(const HTTPRequest* request = nullptr);
// 205 Reset Content
[[maybe_unused]] HTTPResponse responseResetContent(const HTTPRequest* request = nullptr);
// 206 Partial Content
[[maybe_unused]] HTTPResponse responsePartialContent(const HTTPRequest* request = nullptr);
// 400 Bad Request
[[maybe_unused]] HTTPResponse responseBadRequest(const HTTPRequest* request = nullptr);
// 401 Unauthorized
[[maybe_unused]] HTTPResponse responseAuthRequired(const HTTPRequest* request = nullptr);
// 401 Unauthorized with Basic Auth challenge
[[maybe_unused]] HTTPResponse responseUnauthorizedBasicAuth(const std::string& realm = "Restricted Area", const HTTPRequest* request = nullptr);
// 403 Forbidden
[[maybe_unused]] HTTPResponse responseForbidden(const HTTPRequest* request = nullptr);
// 404 Not Found
[[maybe_unused]] HTTPResponse responseNotFound(const HTTPRequest* request = nullptr);
// 405 Method Not Allowed
[[maybe_unused]] HTTPResponse responseMethodNotAllowed(const HTTPRequest* request = nullptr);
// 405 with optional Allow header (comma-separated methods, e.g. "GET, HEAD")
[[maybe_unused]] HTTPResponse responseMethodNotAllowed(const HTTPRequest* request,
                                                       std::string_view allowMethods);
// 409 Conflict
[[maybe_unused]] HTTPResponse responseConflict(const HTTPRequest* request = nullptr);
// 500 Internal Server Error
[[maybe_unused]] HTTPResponse responseInternalServerError(const HTTPRequest* request = nullptr);

}  // namespace geruest

#endif // GERUEST_HTTPRESPONSE_HPP