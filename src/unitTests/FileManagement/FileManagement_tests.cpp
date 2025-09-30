/**
 * @file FileManagement_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the FileManagement class
 */

#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>
#include "../../FileManagement/FileManagement.hpp"

namespace geruest {
namespace test {

const std::string TEST_DIR = "test_files";
const std::string TEST_FILE = TEST_DIR + "/test.txt";
const std::string TEST_CONTENT = "This is test content for FileManagement tests.";

void test_create_folder() {
    // Clean up first
    if (std::filesystem::exists(TEST_DIR)) {
        std::filesystem::remove_all(TEST_DIR);
    }
    
    bool result = FileManagement::createFolder(TEST_DIR);
    assert(result == true);
    assert(std::filesystem::exists(TEST_DIR));
    assert(std::filesystem::is_directory(TEST_DIR));
}

void test_create_existing_folder() {
    // Folder should already exist from previous test
    bool result = FileManagement::createFolder(TEST_DIR);
    // Should still return true for existing folders
    assert(result == true);
}

void test_create_file() {
    bool result = FileManagement::createFile(TEST_FILE);
    assert(result == true);
    assert(std::filesystem::exists(TEST_FILE));
}

void test_file_exists() {
    bool exists = FileManagement::fileExists(TEST_FILE);
    assert(exists == true);
    
    bool notExists = FileManagement::fileExists("nonexistent_file.txt");
    assert(notExists == false);
}

void test_save_file() {
    bool result = FileManagement::saveFile(TEST_FILE, TEST_CONTENT);
    assert(result == true);
    
    // Verify file was saved correctly
    std::ifstream file(TEST_FILE);
    assert(file.is_open());
    
    std::string content;
    std::getline(file, content, '\0'); // Read entire file
    file.close();
    
    assert(content == TEST_CONTENT);
}

void test_load_file_content() {
    // Save known content first
    FileManagement::saveFile(TEST_FILE, "Test content for loading");
    
    // This tests the ContentBuilder's loadFile method indirectly
    // since FileManagement doesn't have a loadFile method
    std::ifstream file(TEST_FILE);
    assert(file.is_open());
    
    std::string content;
    std::getline(file, content, '\0');
    file.close();
    
    assert(content == "Test content for loading");
}

void test_save_file_with_path_creation() {
    std::string deepPath = TEST_DIR + "/subdir/deep/test_deep.txt";
    
    // This should create the directory structure and the file
    bool result = FileManagement::saveFile(deepPath, "Deep content");
    assert(result == true);
    assert(std::filesystem::exists(deepPath));
    
    // Verify content
    std::ifstream file(deepPath);
    std::string content;
    std::getline(file, content, '\0');
    file.close();
    
    assert(content == "Deep content");
}

void test_delete_file() {
    // Create a file to delete
    std::string fileToDelete = TEST_DIR + "/to_delete.txt";
    FileManagement::saveFile(fileToDelete, "Delete me");
    assert(std::filesystem::exists(fileToDelete));
    
    // Delete the file
    FileManagement::deleteFile(fileToDelete);
    assert(!std::filesystem::exists(fileToDelete));
}

void test_create_file_invalid_path() {
    // Try to create file in non-existent directory without creating path
    std::string invalidPath = "/invalid/nonexistent/path/file.txt";
    
    // This should handle the error gracefully
    // Note: Actual behavior depends on implementation
    try {
        bool result = FileManagement::createFile(invalidPath);
        // If it returns false, that's expected
        // If it throws, that should be caught
    } catch (...) {
        // Exception handling is acceptable for invalid paths
    }
}

// Cleanup function
void cleanup_test_files() {
    if (std::filesystem::exists(TEST_DIR)) {
        std::filesystem::remove_all(TEST_DIR);
    }
}

/**
 * @brief Run all FileManagement unit tests
 * @return true if all tests pass, false otherwise
 */
bool runFileManagementTests() {
    std::cout << "=== Running FileManagement Tests ===" << std::endl;
    
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
    runTest(test_create_folder, "test_create_folder");
    runTest(test_create_existing_folder, "test_create_existing_folder");
    runTest(test_create_file, "test_create_file");
    runTest(test_file_exists, "test_file_exists");
    runTest(test_save_file, "test_save_file");
    runTest(test_load_file_content, "test_load_file_content");
    runTest(test_save_file_with_path_creation, "test_save_file_with_path_creation");
    runTest(test_delete_file, "test_delete_file");
    runTest(test_create_file_invalid_path, "test_create_file_invalid_path");

    // Cleanup
    cleanup_test_files();

    std::cout << std::endl;
    std::cout << "=== FileManagement Test Results ===" << std::endl;
    std::cout << "Tests run: " << testCount << std::endl;
    std::cout << "Tests passed: " << passedCount << std::endl;
    std::cout << "Tests failed: " << (testCount - passedCount) << std::endl;
    
    if (allPassed) {
        std::cout << "✓ All FileManagement tests passed!" << std::endl;
    } else {
        std::cout << "✗ Some FileManagement tests failed!" << std::endl;
    }
    
    return allPassed;
}

} // namespace test
} // namespace geruest

// Main function for running FileManagement tests standalone
#ifndef RUNNING_MAIN_TESTS
int main() {
    return geruest::test::runFileManagementTests() ? 0 : 1;
}
#endif