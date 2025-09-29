/**
 * @file HTTPResponse_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the HTTPResponse class
 */

#include <iostream>
#include <cassert>
#include <string>
#include "../../data/HTTPResponse.hpp"
#include "../../data/HTTPRequest.hpp"

namespace geruest {
namespace test {

void test_http_response_creation() {
    HTTPResponse response("200 OK");
    std::string responseStr = response.toString();
    
    // Should contain status line
    assert(responseStr.find("HTTP/1.1 200 OK") != std::string::npos);
}

void test_http_response_set_header() {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "text/html");
    response.setHeader("Content-Length", "13");
    
    std::string responseStr = response.toString();
    
    assert(responseStr.find("Content-Type: text/html") != std::string::npos);
    assert(responseStr.find("Content-Length: 13") != std::string::npos);
}

void test_http_response_set_body() {
    HTTPResponse response("200 OK");
    response.setBody("Hello, World!");
    
    std::string responseStr = response.toString();
    
    // Should contain body
    assert(responseStr.find("Hello, World!") != std::string::npos);
    // Should automatically set Content-Length
    assert(responseStr.find("Content-Length: 13") != std::string::npos);
}

void test_http_response_add_header() {
    HTTPResponse response("200 OK");
    response.addHeader("Set-Cookie", "session=abc123");
    response.addHeader("Set-Cookie", "user=john");
    
    std::string responseStr = response.toString();
    
    // Should contain both Set-Cookie headers
    assert(responseStr.find("Set-Cookie: session=abc123") != std::string::npos);
    assert(responseStr.find("Set-Cookie: user=john") != std::string::npos);
}

void test_http_response_complete_response() {
    HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Server", "Geruest/1.0");
    response.setBody("{\"status\":\"success\"}");
    
    std::string responseStr = response.toString();
    
    // Should have proper HTTP response format
    assert(responseStr.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(responseStr.find("Content-Type: application/json") != std::string::npos);
    assert(responseStr.find("Server: Geruest/1.0") != std::string::npos);
    assert(responseStr.find("Content-Length: 19") != std::string::npos);
    assert(responseStr.find("{\"status\":\"success\"}") != std::string::npos);
    
    // Should have proper HTTP structure (headers followed by blank line, then body)
    assert(responseStr.find("\r\n\r\n") != std::string::npos);
}

void test_predefined_response_functions() {
    // Test some of the predefined response functions
    HTTPResponse okResponse = responseOK();
    assert(okResponse.toString().find("HTTP/1.1 200 OK") != std::string::npos);
    
    HTTPResponse notFoundResponse = responseNotFound();
    assert(notFoundResponse.toString().find("HTTP/1.1 404 Not Found") != std::string::npos);
    
    HTTPResponse badRequestResponse = responseBadRequest();
    assert(badRequestResponse.toString().find("HTTP/1.1 400 Bad Request") != std::string::npos);
}

void test_response_with_request_context() {
    // Create a mock request
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "Origin: https://example.com\r\n"
                            "\r\n";
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    // Test response with request context
    HTTPResponse response = responseOK(&request);
    std::string responseStr = response.toString();
    
    // Should contain CORS headers when Origin is present
    assert(responseStr.find("Access-Control-Allow-Origin: https://example.com") != std::string::npos);
}

void test_build_header_functions() {
    // Test legacy build functions
    std::string badRequestHeader = buildBadRequestHeader();
    assert(badRequestHeader.find("HTTP/1.1 400 Bad Request") != std::string::npos);
    
    std::string notFoundHeader = buildNotFoundHeader();
    assert(notFoundHeader.find("HTTP/1.1 404 Not Found") != std::string::npos);
    
    std::string authHeader = buildAuthHeader();
    assert(authHeader.find("HTTP/1.1 401 Unauthorized") != std::string::npos);
    assert(authHeader.find("WWW-Authenticate") != std::string::npos);
}

/**
 * @brief Run all HTTPResponse unit tests
 * @return true if all tests pass, false otherwise
 */
bool runHTTPResponseTests() {
    std::cout << "=== Running HTTPResponse Tests ===" << std::endl;
    
    bool allPassed = true;
    int testCount = 0;
    int passedCount = 0;

    // Helper lambda to run a test and track results
    auto runTest = [&](void (*testFunc)(), const std::string& testName) {
        testCount++;
        try {
            testFunc();
            std::cout << "✓ " << testName << " passed" << std::endl;
            passedCount++;
        } catch (const std::exception& e) {
            std::cout << "✗ " << testName << " failed: " << e.what() << std::endl;
            allPassed = false;
        } catch (...) {
            std::cout << "✗ " << testName << " failed: Unknown error" << std::endl;
            allPassed = false;
        }
    };

    // Run all test functions
    runTest(test_http_response_creation, "test_http_response_creation");
    runTest(test_http_response_set_header, "test_http_response_set_header");
    runTest(test_http_response_set_body, "test_http_response_set_body");
    runTest(test_http_response_add_header, "test_http_response_add_header");
    runTest(test_http_response_complete_response, "test_http_response_complete_response");
    runTest(test_predefined_response_functions, "test_predefined_response_functions");
    runTest(test_response_with_request_context, "test_response_with_request_context");
    runTest(test_build_header_functions, "test_build_header_functions");

    std::cout << std::endl;
    std::cout << "=== HTTPResponse Test Results ===" << std::endl;
    std::cout << "Tests run: " << testCount << std::endl;
    std::cout << "Tests passed: " << passedCount << std::endl;
    std::cout << "Tests failed: " << (testCount - passedCount) << std::endl;
    
    if (allPassed) {
        std::cout << "✓ All HTTPResponse tests passed!" << std::endl;
    } else {
        std::cout << "✗ Some HTTPResponse tests failed!" << std::endl;
    }
    
    return allPassed;
}

} // namespace test
} // namespace geruest

// Main function for running HTTPResponse tests standalone
#ifndef RUNNING_MAIN_TESTS
int main() {
    return geruest::test::runHTTPResponseTests() ? 0 : 1;
}
#endif