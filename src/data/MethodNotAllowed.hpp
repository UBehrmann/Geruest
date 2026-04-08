/**
 * @file MethodNotAllowed.hpp
 * @brief Exception thrown by route handlers when the HTTP method is not supported for the route.
 */

#ifndef GERUEST_METHOD_NOT_ALLOWED_HPP
#define GERUEST_METHOD_NOT_ALLOWED_HPP

#include <exception>
#include <string>

namespace geruest {

/**
 * Thrown from a route handler to signal the framework should respond with 405 Method Not Allowed.
 * Optionally carries an RFC 9110 Allow field value (e.g. "GET, HEAD").
 */
class method_not_allowed : public std::exception {
   public:
    explicit method_not_allowed(std::string allowMethods = {});

    const char* what() const noexcept override;
    const std::string& allowMethods() const noexcept { return _allow; }

   private:
    std::string _allow;
    std::string _what;
};

}  // namespace geruest

#endif  // GERUEST_METHOD_NOT_ALLOWED_HPP
