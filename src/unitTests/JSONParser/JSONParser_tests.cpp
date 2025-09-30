/**
 * @file JSONParser_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the JSONParser class
 */

#include <iostream>
#include "testFunctions.hpp"

namespace geruest {
namespace test {

/**
 * @brief Run all JSONParser unit tests
 * @return true if all tests pass, false otherwise
 */
bool runJSONParserTests() {
    std::cout << "=== Running JSONParser Tests ===" << std::endl;
    
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
    runTest(test_simple_key_strings, "test_simple_key_strings");
    runTest(test_simple_key_integers1, "test_simple_key_integers1");
    runTest(test_simple_key_integers2, "test_simple_key_integers2");
    runTest(test_simple_key_integers3, "test_simple_key_integers3");
    runTest(test_simple_key_integers4, "test_simple_key_integers4");
    runTest(test_simple_key_floats1, "test_simple_key_floats1");
    runTest(test_simple_key_floats2, "test_simple_key_floats2");
    runTest(test_simple_key_floats3, "test_simple_key_floats3");
    runTest(test_simple_key_booleans, "test_simple_key_booleans");
    runTest(test_simple_key_null, "test_simple_key_null");
    runTest(test_multiple_key_value_pairs, "test_multiple_key_value_pairs");
    runTest(test_nested_objects, "test_nested_objects");
    runTest(test_array_of_values_strings, "test_array_of_values_strings");
    runTest(test_array_of_values_shorts, "test_array_of_values_shorts");
    runTest(test_array_of_values_integers, "test_array_of_values_integers");
    runTest(test_array_of_values_longs, "test_array_of_values_longs");
    runTest(test_array_of_values_longlongs, "test_array_of_values_longlongs");
    runTest(test_array_of_values_floats, "test_array_of_values_floats");
    runTest(test_array_of_values_doubles, "test_array_of_values_doubles");
    runTest(test_array_of_values_longdoubles, "test_array_of_values_longdoubles");
    runTest(test_array_of_values_booleans, "test_array_of_values_booleans");
    runTest(test_mixed_data_types, "test_mixed_data_types");
    runTest(test_empty_object, "test_empty_object");
    runTest(test_invalid_json, "test_invalid_json");
    runTest(test_boolean_values, "test_boolean_values");
    runTest(test_large_json_object, "test_large_json_object");
    runTest(test_json_from_file, "test_json_from_file");
    runTest(test_toString_json, "test_toString_json");
    runTest(test_nested_json_from_file, "test_nested_json_from_file");
    runTest(test_save_to_file, "test_save_to_file");
    runTest(test_double_nested_json_from_file, "test_double_nested_json_from_file");
    runTest(test_set_values, "test_set_values");
    runTest(test_json_array_from_file, "test_json_array_from_file");
    runTest(test_json_array_with_one_element, "test_json_array_with_one_element");
    runTest(test_adding_json_array_to_json_array, "test_adding_json_array_to_json_array");
    runTest(test_functions_file, "test_functions_file");
    runTest(test_add_to_array_and_array_to_string, "test_add_to_array_and_array_to_string");
    runTest(test_add_to_empty_array_and_array_to_string, "test_add_to_empty_array_and_array_to_string");

    std::cout << std::endl;
    std::cout << "=== JSONParser Test Results ===" << std::endl;
    std::cout << "Tests run: " << testCount << std::endl;
    std::cout << "Tests passed: " << passedCount << std::endl;
    std::cout << "Tests failed: " << (testCount - passedCount) << std::endl;
    
    if (allPassed) {
        std::cout << "✓ All JSONParser tests passed!" << std::endl;
    } else {
        std::cout << "✗ Some JSONParser tests failed!" << std::endl;
    }
    
    return allPassed;
}

} // namespace test
} // namespace geruest

// Main function for running JSONParser tests standalone
#ifndef RUNNING_MAIN_TESTS
int main() {
    return geruest::test::runJSONParserTests() ? 0 : 1;
}
#endif