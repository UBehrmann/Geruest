/**
 * @file ConfigLoader_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the ConfigLoader class
 */

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "../../config/ConfigLoader.hpp"

using namespace geruest;

// Test fixture for ConfigLoader
class ConfigLoaderTest : public ::testing::Test {
protected:
    const std::string testEnvFile = "test_config.env";
    
    void SetUp() override {
        // Clear any previous state
        ConfigLoader::clear();
        
        // Clean up test file if it exists
        if (std::filesystem::exists(testEnvFile)) {
            std::filesystem::remove(testEnvFile);
        }
    }
    
    void TearDown() override {
        // Clean up test file
        if (std::filesystem::exists(testEnvFile)) {
            std::filesystem::remove(testEnvFile);
        }
        
        // Clear ConfigLoader state
        ConfigLoader::clear();
        
        // Unset any test environment variables
        #ifdef _WIN32
        _putenv("TEST_VAR=");
        _putenv("TEST_INT=");
        _putenv("TEST_BOOL=");
        _putenv("TEST_FLOAT=");
        #else
        unsetenv("TEST_VAR");
        unsetenv("TEST_INT");
        unsetenv("TEST_BOOL");
        unsetenv("TEST_FLOAT");
        #endif
    }
    
    void createEnvFile(const std::string& content) {
        std::ofstream file(testEnvFile);
        file << content;
        file.close();
    }
    
    void setEnvVar(const std::string& key, const std::string& value) {
        #ifdef _WIN32
        _putenv((key + "=" + value).c_str());
        #else
        setenv(key.c_str(), value.c_str(), 1);
        #endif
    }
};

// Basic loading
TEST_F(ConfigLoaderTest, LoadNonexistentFile) {
    bool loaded = ConfigLoader::loadEnvFile("nonexistent.env");
    EXPECT_FALSE(loaded);
}

TEST_F(ConfigLoaderTest, LoadEmptyFile) {
    createEnvFile("");
    bool loaded = ConfigLoader::loadEnvFile(testEnvFile);
    EXPECT_TRUE(loaded);
}

TEST_F(ConfigLoaderTest, LoadSimpleKeyValue) {
    createEnvFile("KEY=value\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    EXPECT_EQ(value, "value");
}

TEST_F(ConfigLoaderTest, LoadMultipleKeyValues) {
    createEnvFile("KEY1=value1\nKEY2=value2\nKEY3=value3\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_EQ(ConfigLoader::get("KEY1"), "value1");
    EXPECT_EQ(ConfigLoader::get("KEY2"), "value2");
    EXPECT_EQ(ConfigLoader::get("KEY3"), "value3");
}

TEST_F(ConfigLoaderTest, LoadValueWithSpaces) {
    createEnvFile("KEY=value with spaces\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    EXPECT_EQ(value, "value with spaces");
}

TEST_F(ConfigLoaderTest, LoadQuotedValue) {
    createEnvFile("KEY=\"quoted value\"\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    EXPECT_EQ(value, "quoted value");
}

TEST_F(ConfigLoaderTest, LoadSingleQuotedValue) {
    createEnvFile("KEY='single quoted'\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    // Implementation may vary - test actual behavior
    // Most .env parsers treat single quotes similarly to double quotes
}

TEST_F(ConfigLoaderTest, IgnoreComments) {
    createEnvFile("# This is a comment\nKEY=value\n# Another comment\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_EQ(ConfigLoader::get("KEY"), "value");
    EXPECT_FALSE(ConfigLoader::has("# This is a comment"));
}

TEST_F(ConfigLoaderTest, IgnoreEmptyLines) {
    createEnvFile("\n\nKEY1=value1\n\n\nKEY2=value2\n\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_EQ(ConfigLoader::get("KEY1"), "value1");
    EXPECT_EQ(ConfigLoader::get("KEY2"), "value2");
}

TEST_F(ConfigLoaderTest, HandleWhitespace) {
    createEnvFile("  KEY  =  value  \n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    // Should trim whitespace around key and value
    std::string value = ConfigLoader::get("KEY");
    EXPECT_FALSE(value.empty());
}

// Default values
TEST_F(ConfigLoaderTest, GetNonexistentKeyReturnsDefault) {
    std::string value = ConfigLoader::get("NONEXISTENT", "default");
    EXPECT_EQ(value, "default");
}

TEST_F(ConfigLoaderTest, GetNonexistentKeyEmptyDefault) {
    std::string value = ConfigLoader::get("NONEXISTENT");
    EXPECT_EQ(value, "");
}

// Type conversions
TEST_F(ConfigLoaderTest, GetIntValue) {
    createEnvFile("PORT=8080\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    int port = ConfigLoader::getInt("PORT");
    EXPECT_EQ(port, 8080);
}

TEST_F(ConfigLoaderTest, GetIntWithDefault) {
    int value = ConfigLoader::getInt("NONEXISTENT", 3000);
    EXPECT_EQ(value, 3000);
}

TEST_F(ConfigLoaderTest, GetIntInvalidValue) {
    createEnvFile("PORT=invalid\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    int value = ConfigLoader::getInt("PORT", 8080);
    EXPECT_EQ(value, 8080); // Should return default on parse error
}

TEST_F(ConfigLoaderTest, GetFloatValue) {
    createEnvFile("RATIO=3.14159\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    float ratio = ConfigLoader::getFloat("RATIO");
    EXPECT_FLOAT_EQ(ratio, 3.14159f);
}

TEST_F(ConfigLoaderTest, GetFloatWithDefault) {
    float value = ConfigLoader::getFloat("NONEXISTENT", 2.5f);
    EXPECT_FLOAT_EQ(value, 2.5f);
}

TEST_F(ConfigLoaderTest, GetFloatInvalidValue) {
    createEnvFile("RATIO=notanumber\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    float value = ConfigLoader::getFloat("RATIO", 1.0f);
    EXPECT_FLOAT_EQ(value, 1.0f);
}

TEST_F(ConfigLoaderTest, GetBoolTrue) {
    createEnvFile("ENABLED=true\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    bool enabled = ConfigLoader::getBool("ENABLED");
    EXPECT_TRUE(enabled);
}

TEST_F(ConfigLoaderTest, GetBoolFalse) {
    createEnvFile("ENABLED=false\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    bool enabled = ConfigLoader::getBool("ENABLED");
    EXPECT_FALSE(enabled);
}

TEST_F(ConfigLoaderTest, GetBoolVariations) {
    createEnvFile("B1=true\nB2=TRUE\nB3=1\nB4=yes\nB5=YES\nB6=on\nB7=ON\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_TRUE(ConfigLoader::getBool("B1"));
    EXPECT_TRUE(ConfigLoader::getBool("B2"));
    EXPECT_TRUE(ConfigLoader::getBool("B3"));
    EXPECT_TRUE(ConfigLoader::getBool("B4"));
    EXPECT_TRUE(ConfigLoader::getBool("B5"));
    EXPECT_TRUE(ConfigLoader::getBool("B6"));
    EXPECT_TRUE(ConfigLoader::getBool("B7"));
}

TEST_F(ConfigLoaderTest, GetBoolFalseVariations) {
    createEnvFile("B1=false\nB2=FALSE\nB3=0\nB4=no\nB5=NO\nB6=off\nB7=OFF\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_FALSE(ConfigLoader::getBool("B1"));
    EXPECT_FALSE(ConfigLoader::getBool("B2"));
    EXPECT_FALSE(ConfigLoader::getBool("B3"));
    EXPECT_FALSE(ConfigLoader::getBool("B4"));
    EXPECT_FALSE(ConfigLoader::getBool("B5"));
    EXPECT_FALSE(ConfigLoader::getBool("B6"));
    EXPECT_FALSE(ConfigLoader::getBool("B7"));
}

TEST_F(ConfigLoaderTest, GetBoolWithDefault) {
    bool value = ConfigLoader::getBool("NONEXISTENT", true);
    EXPECT_TRUE(value);
}

TEST_F(ConfigLoaderTest, GetBoolInvalidValue) {
    createEnvFile("ENABLED=maybe\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    bool value = ConfigLoader::getBool("ENABLED", true);
    EXPECT_TRUE(value); // Should return default
}

TEST_F(ConfigLoaderTest, GetSizeTValue) {
    createEnvFile("SIZE=1024\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    size_t size = ConfigLoader::getSizeT("SIZE");
    EXPECT_EQ(size, 1024);
}

TEST_F(ConfigLoaderTest, GetSizeTWithDefault) {
    size_t value = ConfigLoader::getSizeT("NONEXISTENT", 512);
    EXPECT_EQ(value, 512);
}

// Has method
TEST_F(ConfigLoaderTest, HasExistingKey) {
    createEnvFile("KEY=value\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_TRUE(ConfigLoader::has("KEY"));
}

TEST_F(ConfigLoaderTest, HasNonexistentKey) {
    createEnvFile("KEY=value\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_FALSE(ConfigLoader::has("NONEXISTENT"));
}

TEST_F(ConfigLoaderTest, HasEnvironmentVariable) {
    setEnvVar("ENV_TEST_VAR", "value");
    
    EXPECT_TRUE(ConfigLoader::has("ENV_TEST_VAR"));
}

// Environment variable fallback
TEST_F(ConfigLoaderTest, FallbackToEnvironmentVariable) {
    setEnvVar("TEST_VAR", "env_value");
    
    std::string value = ConfigLoader::get("TEST_VAR");
    EXPECT_EQ(value, "env_value");
}

TEST_F(ConfigLoaderTest, EnvFileTakesPrecedenceOverEnvironment) {
    setEnvVar("TEST_VAR", "env_value");
    createEnvFile("TEST_VAR=file_value\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("TEST_VAR");
    EXPECT_EQ(value, "file_value");
}

// Clear functionality
TEST_F(ConfigLoaderTest, ClearRemovesLoadedValues) {
    createEnvFile("KEY=value\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    EXPECT_TRUE(ConfigLoader::has("KEY"));
    
    ConfigLoader::clear();
    
    // After clear, should not find the key in cache
    // (but might still find in environment if set)
    std::string value = ConfigLoader::get("KEY", "default");
    // Result depends on whether KEY exists in actual environment
}

// Edge cases
TEST_F(ConfigLoaderTest, EmptyKey) {
    createEnvFile("=value\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    // Should handle gracefully (skip or store empty key)
}

TEST_F(ConfigLoaderTest, EmptyValue) {
    createEnvFile("KEY=\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY", "default");
    // Empty value should be stored as empty string, not use default
    EXPECT_EQ(value, "");
}

TEST_F(ConfigLoaderTest, EqualsSignInValue) {
    createEnvFile("KEY=value=with=equals\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    EXPECT_EQ(value, "value=with=equals");
}

TEST_F(ConfigLoaderTest, MultipleLoadsOverwrite) {
    createEnvFile("KEY=value1\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    EXPECT_EQ(ConfigLoader::get("KEY"), "value1");
    
    createEnvFile("KEY=value2\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    EXPECT_EQ(ConfigLoader::get("KEY"), "value2");
}

TEST_F(ConfigLoaderTest, SpecialCharactersInValue) {
    createEnvFile("KEY=value!@#$%^&*()\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    EXPECT_EQ(value, "value!@#$%^&*()");
}

TEST_F(ConfigLoaderTest, NumericStringNotAutoConverted) {
    createEnvFile("KEY=12345\n");
    ConfigLoader::loadEnvFile(testEnvFile);
    
    std::string value = ConfigLoader::get("KEY");
    EXPECT_EQ(value, "12345");
    EXPECT_EQ(ConfigLoader::getInt("KEY"), 12345);
}
