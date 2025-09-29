/**
 * @file unit_tests.cpp
 * @created 2024-06-06
 * @updated 2024-09-29
 * @author Urs Behrmann
 * @brief Main unit test runner for the Geruest framework
 */

#include <iostream>

// Define this to prevent individual test main functions from running
#define RUNNING_MAIN_TESTS

// Include all test modules
#include "JSONParser/JSONParser_tests.cpp"
#include "HTTPRequest/HTTPRequest_tests.cpp"
#include "HTTPResponse/HTTPResponse_tests.cpp"
#include "FileManagement/FileManagement_tests.cpp"
#include "ContentBuilder/ContentBuilder_tests.cpp"

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "    Geruest Framework Unit Tests" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << std::endl;

    bool allTestsPassed = true;
    int totalModules = 0;
    int passedModules = 0;

    // Run JSONParser tests
    totalModules++;
    if (geruest::test::runJSONParserTests()) {
        passedModules++;
    } else {
        allTestsPassed = false;
    }

    std::cout << std::endl;

    // Run HTTPRequest tests
    totalModules++;
    if (geruest::test::runHTTPRequestTests()) {
        passedModules++;
    } else {
        allTestsPassed = false;
    }

    std::cout << std::endl;

    // Run HTTPResponse tests
    totalModules++;
    if (geruest::test::runHTTPResponseTests()) {
        passedModules++;
    } else {
        allTestsPassed = false;
    }

    std::cout << std::endl;

    // Run FileManagement tests
    totalModules++;
    if (geruest::test::runFileManagementTests()) {
        passedModules++;
    } else {
        allTestsPassed = false;
    }

    std::cout << std::endl;

    // Run ContentBuilder tests
    totalModules++;
    if (geruest::test::runContentBuilderTests()) {
        passedModules++;
    } else {
        allTestsPassed = false;
    }

    std::cout << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "         Final Test Results" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "Test modules run: " << totalModules << std::endl;
    std::cout << "Test modules passed: " << passedModules << std::endl;
    std::cout << "Test modules failed: " << (totalModules - passedModules) << std::endl;
    std::cout << std::endl;

    if (allTestsPassed) {
        std::cout << "🎉 All unit tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Some unit tests failed!" << std::endl;
        return 1;
    }
}
