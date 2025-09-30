/**
 * @file HTTPRequest_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the HTTPRequest class
 */

#include <iostream>
#include <cassert>
#include <string>
#include "../../data/HTTPRequest.hpp"

namespace geruest {
namespace test {

void test_http_request_parsing() {
    std::string rawRequest = "GET /test HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "User-Agent: Test/1.0\r\n"
                            "Content-Length: 4\r\n"
                            "\r\n"
                            "test";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    assert(request.getMethod() == "GET");
    assert(request.getPathString() == "/test");
    // assert(request.getVersion() == "HTTP/1.1"); // getVersion() method not available
    assert(request.hasHeader("host")); // Headers should be case-insensitive
    assert(request.getHeader("host") == "example.com");
    assert(request.getHeader("user-agent") == "Test/1.0");
    assert(request.getBody() == "test");
}

void test_http_request_post_with_body() {
    std::string rawRequest = "POST /api/users HTTP/1.1\r\n"
                            "Host: api.example.com\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: 25\r\n"
                            "\r\n"
                            "{\"name\":\"John Doe\"}";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    assert(request.getMethod() == "POST");
    assert(request.getPathString() == "/api/users");
    assert(request.getHeader("content-type") == "application/json");
    assert(request.getBody() == "{\"name\":\"John Doe\"}");
}

void test_http_request_query_parameters() {
    std::string rawRequest = "GET /search?q=test&limit=10 HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "\r\n";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    assert(request.getMethod() == "GET");
    assert(request.getPathString() == "/search?q=test&limit=10");
}

void test_http_request_url_decode() {
    // Test the urlDecode function
    std::string encoded = "Hello%20World%21";
    std::string decoded = urlDecode(encoded);
    assert(decoded == "Hello World!");
}

void test_http_request_trim_function() {
    // Test static utility functions if accessible
    std::string input = "  test  ";
    // Note: HTTPRequest::trim is private, so we can't test it directly
    // This would need to be made public or tested through integration
}

void test_http_request_empty_body() {
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "\r\n";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    assert(request.getMethod() == "GET");
    assert(request.getPathString() == "/");
    assert(request.getBody().empty());
}

void test_http_request_case_insensitive_headers() {
    std::string rawRequest = "GET / HTTP/1.1\r\n"
                            "HOST: example.com\r\n"
                            "USER-AGENT: Test/1.0\r\n"
                            "\r\n";
    
    HTTPRequest request(rawRequest, "127.0.0.1", "/test/root");
    
    assert(request.hasHeader("host"));
    assert(request.hasHeader("HOST"));
    assert(request.hasHeader("Host"));
    assert(request.getHeader("host") == "example.com");
}

/**
 * @brief Run all HTTPRequest unit tests
 * @return true if all tests pass, false otherwise
 */
bool runHTTPRequestTests() {
    std::cout << "=== Running HTTPRequest Tests ===" << std::endl;
    
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
    runTest(test_http_request_parsing, "test_http_request_parsing");
    runTest(test_http_request_post_with_body, "test_http_request_post_with_body");
    runTest(test_http_request_query_parameters, "test_http_request_query_parameters");
    runTest(test_http_request_url_decode, "test_http_request_url_decode");
    runTest(test_http_request_empty_body, "test_http_request_empty_body");
    runTest(test_http_request_case_insensitive_headers, "test_http_request_case_insensitive_headers");

    std::cout << std::endl;
    std::cout << "=== HTTPRequest Test Results ===" << std::endl;
    std::cout << "Tests run: " << testCount << std::endl;
    std::cout << "Tests passed: " << passedCount << std::endl;
    std::cout << "Tests failed: " << (testCount - passedCount) << std::endl;
    
    if (allPassed) {
        std::cout << "✓ All HTTPRequest tests passed!" << std::endl;
    } else {
        std::cout << "✗ Some HTTPRequest tests failed!" << std::endl;
    }
    
    return allPassed;
}

} // namespace test
} // namespace geruest

// Main function for running HTTPRequest tests standalone
#ifndef RUNNING_MAIN_TESTS
int main() {
    return geruest::test::runHTTPRequestTests() ? 0 : 1;
}
#endif