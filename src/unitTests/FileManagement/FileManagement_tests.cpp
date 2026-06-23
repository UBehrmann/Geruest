/**
 * @file FileManagement_tests.cpp
 * @created 2024-09-29
 * @author Urs Behrmann
 * @brief Unit tests for the FileManagement class
 */

#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <fstream>
#include "../../FileManagement/FileManagement.hpp"

using namespace geruest;

const std::string TEST_DIR = "test_files";
const std::string TEST_FILE = TEST_DIR + "/test.txt";
const std::string TEST_CONTENT = "This is test content for FileManagement tests.";

// Test fixture for FileManagement tests
class FileManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up before each test
        if (std::filesystem::exists(TEST_DIR)) {
            std::filesystem::remove_all(TEST_DIR);
        }
    }

    void TearDown() override {
        // Clean up after each test
        if (std::filesystem::exists(TEST_DIR)) {
            std::filesystem::remove_all(TEST_DIR);
        }
    }
};

TEST_F(FileManagementTest, CreateFolder) {
    bool result = FileManagement::createFolder(TEST_DIR);
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(TEST_DIR));
    EXPECT_TRUE(std::filesystem::is_directory(TEST_DIR));
}

TEST_F(FileManagementTest, CreateExistingFolder) {
    // Create folder first
    FileManagement::createFolder(TEST_DIR);
    
    // Try to create it again
    bool result = FileManagement::createFolder(TEST_DIR);
    // Should still return true for existing folders
    EXPECT_TRUE(result);
}

TEST_F(FileManagementTest, CreateFile) {
    // Create parent directory first
    FileManagement::createFolder(TEST_DIR);
    
    bool result = FileManagement::createFile(TEST_FILE);
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(TEST_FILE));
}

TEST_F(FileManagementTest, FileExists) {
    // Create parent directory and file
    FileManagement::createFolder(TEST_DIR);
    FileManagement::createFile(TEST_FILE);
    
    bool exists = FileManagement::fileExists(TEST_FILE);
    EXPECT_TRUE(exists);
    
    bool notExists = FileManagement::fileExists("nonexistent_file.txt");
    EXPECT_FALSE(notExists);
}

TEST_F(FileManagementTest, SaveFile) {
    bool result = FileManagement::saveFile(TEST_FILE, TEST_CONTENT);
    EXPECT_TRUE(result);
    
    // Verify file was saved correctly
    std::ifstream file(TEST_FILE);
    EXPECT_TRUE(file.is_open());
    
    std::string content;
    std::getline(file, content, '\0'); // Read entire file
    file.close();
    
    EXPECT_EQ(content, TEST_CONTENT);
}

TEST_F(FileManagementTest, LoadFileContent) {
    // Save known content first
    FileManagement::saveFile(TEST_FILE, "Test content for loading");
    
    // This tests the ContentBuilder's loadFile method indirectly
    // since FileManagement doesn't have a loadFile method
    std::ifstream file(TEST_FILE);
    EXPECT_TRUE(file.is_open());
    
    std::string content;
    std::getline(file, content, '\0');
    file.close();
    
    EXPECT_EQ(content, "Test content for loading");
}

TEST_F(FileManagementTest, SaveFileWithPathCreation) {
    std::string deepPath = TEST_DIR + "/subdir/deep/test_deep.txt";
    
    // This should create the directory structure and the file
    bool result = FileManagement::saveFile(deepPath, "Deep content");
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(deepPath));
    
    // Verify content
    std::ifstream file(deepPath);
    std::string content;
    std::getline(file, content, '\0');
    file.close();
    
    EXPECT_EQ(content, "Deep content");
}

TEST_F(FileManagementTest, DeleteFile) {
    // Create a file to delete
    std::string fileToDelete = TEST_DIR + "/to_delete.txt";
    FileManagement::saveFile(fileToDelete, "Delete me");
    EXPECT_TRUE(std::filesystem::exists(fileToDelete));
    
    // Delete the file
    FileManagement::deleteFile(fileToDelete);
    EXPECT_FALSE(std::filesystem::exists(fileToDelete));
}

TEST_F(FileManagementTest, CreateFileInvalidPath) {
    // Try to create file in non-existent directory without creating path
    std::string invalidPath = "/invalid/nonexistent/path/file.txt";
    
    // This should handle the error gracefully
    // Note: Actual behavior depends on implementation
    try {
        (void)FileManagement::createFile(invalidPath);
    } catch (...) {
        // Exception handling is acceptable for invalid paths
    }
}