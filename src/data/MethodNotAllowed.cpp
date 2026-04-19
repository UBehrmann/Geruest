/**
 * @file MethodNotAllowed.cpp
 */

#include "MethodNotAllowed.hpp"

namespace geruest {

method_not_allowed::method_not_allowed(std::string allowMethods)
    : _allow(std::move(allowMethods)),
      _what(_allow.empty() ? std::string("405 Method Not Allowed")
                           : ("405 Method Not Allowed (Allow: " + _allow + ")")) {}

const char* method_not_allowed::what() const noexcept {
    return _what.c_str();
}

}  // namespace geruest
