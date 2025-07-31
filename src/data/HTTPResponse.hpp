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

#include <string>
#include <map>
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
[[maybe_unused]] HTTPResponse responseOK(HTTPRequest* request = nullptr);
// 201 Created
[[maybe_unused]] HTTPResponse responseCreated(HTTPRequest* request = nullptr);
// 202 Accepted
[[maybe_unused]] HTTPResponse responseAccepted(HTTPRequest* request = nullptr);
// 203 Non-Authoritative Information
[[maybe_unused]] HTTPResponse responseNonAuthoritative(HTTPRequest* request = nullptr);
// 204 No Content
[[maybe_unused]] HTTPResponse responseNoContent(HTTPRequest* request = nullptr);
// 205 Reset Content
[[maybe_unused]] HTTPResponse responseResetContent(HTTPRequest* request = nullptr);
// 206 Partial Content
[[maybe_unused]] HTTPResponse responsePartialContent(HTTPRequest* request = nullptr);
// 400 Bad Request
[[maybe_unused]] HTTPResponse responseBadRequest(HTTPRequest* request = nullptr);
// 401 Unauthorized
[[maybe_unused]] HTTPResponse responseAuthRequired(HTTPRequest* request = nullptr);
// 403 Forbidden
[[maybe_unused]] HTTPResponse responseForbidden(HTTPRequest* request = nullptr);
// 404 Not Found
[[maybe_unused]] HTTPResponse responseNotFound(HTTPRequest* request = nullptr);
// 405 Method Not Allowed
[[maybe_unused]] HTTPResponse responseMethodNotAllowed(HTTPRequest* request = nullptr);
// 409 Conflict
[[maybe_unused]] HTTPResponse responseConflict(HTTPRequest* request = nullptr);
// 500 Internal Server Error
[[maybe_unused]] HTTPResponse responseInternalServerError(HTTPRequest* request = nullptr);

}  // namespace geruest

#endif // GERUEST_HTTPRESPONSE_HPP