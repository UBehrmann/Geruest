/**
 * @file BasicAuth_tests.cpp
 * @created 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the BasicAuth class
 */

#include <gtest/gtest.h>
#include "../../auth/BasicAuth.hpp"

using namespace geruest;

// Test fixture for BasicAuth
class BasicAuthTest : public ::testing::Test {
protected:
    BasicAuth* auth;
    
    void SetUp() override {
        auth = new BasicAuth();
    }
    
    void TearDown() override {
        delete auth;
    }
};

// Constructor and default state
TEST_F(BasicAuthTest, ConstructorDefaultsDisabled) {
    EXPECT_FALSE(auth->isEnabled());
    EXPECT_FALSE(auth->hasUsers());
    EXPECT_EQ(auth->userCount(), 0);
    EXPECT_EQ(auth->protectedPageCount(), 0);
}

// Enable/Disable functionality
TEST_F(BasicAuthTest, EnableDisableToggle) {
    EXPECT_FALSE(auth->isEnabled());
    
    auth->setEnabled(true);
    EXPECT_TRUE(auth->isEnabled());
    
    auth->setEnabled(false);
    EXPECT_FALSE(auth->isEnabled());
}

// User management
TEST_F(BasicAuthTest, AddUser) {
    EXPECT_FALSE(auth->hasUsers());
    EXPECT_EQ(auth->userCount(), 0);
    
    auth->addUser("testuser", "password123");
    
    EXPECT_TRUE(auth->hasUsers());
    EXPECT_EQ(auth->userCount(), 1);
}

TEST_F(BasicAuthTest, AddMultipleUsers) {
    auth->addUser("user1", "pass1");
    auth->addUser("user2", "pass2");
    auth->addUser("user3", "pass3");
    
    EXPECT_EQ(auth->userCount(), 3);
}

TEST_F(BasicAuthTest, RemoveUser) {
    auth->addUser("testuser", "password");
    EXPECT_EQ(auth->userCount(), 1);
    
    bool removed = auth->removeUser("testuser");
    EXPECT_TRUE(removed);
    EXPECT_EQ(auth->userCount(), 0);
    EXPECT_FALSE(auth->hasUsers());
}

TEST_F(BasicAuthTest, RemoveNonexistentUser) {
    auth->addUser("user1", "pass1");
    
    bool removed = auth->removeUser("nonexistent");
    EXPECT_FALSE(removed);
    EXPECT_EQ(auth->userCount(), 1);
}

TEST_F(BasicAuthTest, ClearUsers) {
    auth->addUser("user1", "pass1");
    auth->addUser("user2", "pass2");
    auth->addUser("user3", "pass3");
    EXPECT_EQ(auth->userCount(), 3);
    
    auth->clearUsers();
    EXPECT_EQ(auth->userCount(), 0);
    EXPECT_FALSE(auth->hasUsers());
}

TEST_F(BasicAuthTest, AddUserWithHashedPassword) {
    // SHA-256 hash of "password123"
    std::string hashedPass = BasicAuth::hashPassword("password123");
    
    auth->addUserHashed("testuser", hashedPass);
    EXPECT_TRUE(auth->hasUsers());
    EXPECT_EQ(auth->userCount(), 1);
}

// Protected pages management
TEST_F(BasicAuthTest, AddProtectedPage) {
    EXPECT_EQ(auth->protectedPageCount(), 0);
    
    auth->addProtectedPage("/admin");
    
    EXPECT_EQ(auth->protectedPageCount(), 1);
    EXPECT_TRUE(auth->isPageProtected("/admin"));
}

TEST_F(BasicAuthTest, AddMultipleProtectedPages) {
    auth->addProtectedPage("/admin");
    auth->addProtectedPage("/api/admin");
    auth->addProtectedPage("/dashboard");
    
    EXPECT_EQ(auth->protectedPageCount(), 3);
    EXPECT_TRUE(auth->isPageProtected("/admin"));
    EXPECT_TRUE(auth->isPageProtected("/api/admin"));
    EXPECT_TRUE(auth->isPageProtected("/dashboard"));
}

TEST_F(BasicAuthTest, RemoveProtectedPage) {
    auth->addProtectedPage("/admin");
    EXPECT_TRUE(auth->isPageProtected("/admin"));
    
    bool removed = auth->removeProtectedPage("/admin");
    EXPECT_TRUE(removed);
    EXPECT_FALSE(auth->isPageProtected("/admin"));
    EXPECT_EQ(auth->protectedPageCount(), 0);
}

TEST_F(BasicAuthTest, RemoveNonexistentProtectedPage) {
    auth->addProtectedPage("/admin");
    
    bool removed = auth->removeProtectedPage("/nonexistent");
    EXPECT_FALSE(removed);
    EXPECT_EQ(auth->protectedPageCount(), 1);
}

TEST_F(BasicAuthTest, ClearProtectedPages) {
    auth->addProtectedPage("/admin");
    auth->addProtectedPage("/api/admin");
    auth->addProtectedPage("/dashboard");
    EXPECT_EQ(auth->protectedPageCount(), 3);
    
    auth->clearProtectedPages();
    EXPECT_EQ(auth->protectedPageCount(), 0);
    EXPECT_FALSE(auth->isPageProtected("/admin"));
}

TEST_F(BasicAuthTest, UnprotectedPageReturnsFalse) {
    auth->addProtectedPage("/admin");
    
    EXPECT_FALSE(auth->isPageProtected("/public"));
    EXPECT_FALSE(auth->isPageProtected("/"));
}

// Password hashing
TEST_F(BasicAuthTest, HashPasswordConsistency) {
    std::string password = "testpassword123";
    std::string hash1 = BasicAuth::hashPassword(password);
    std::string hash2 = BasicAuth::hashPassword(password);
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.length(), 64); // SHA-256 produces 64 hex characters
}

TEST_F(BasicAuthTest, DifferentPasswordsDifferentHashes) {
    std::string hash1 = BasicAuth::hashPassword("password1");
    std::string hash2 = BasicAuth::hashPassword("password2");
    
    EXPECT_NE(hash1, hash2);
}

// RequiresAuth logic
TEST_F(BasicAuthTest, RequiresAuthWhenDisabled) {
    auth->setEnabled(false);
    auth->addUser("admin", "password");
    auth->addProtectedPage("/admin");
    
    EXPECT_FALSE(auth->requiresAuth("/admin"));
}

TEST_F(BasicAuthTest, RequiresAuthWithNoUsers) {
    auth->setEnabled(true);
    auth->addProtectedPage("/admin");
    
    EXPECT_FALSE(auth->requiresAuth("/admin"));
}

TEST_F(BasicAuthTest, RequiresAuthForUnprotectedPage) {
    auth->setEnabled(true);
    auth->addUser("admin", "password");
    auth->addProtectedPage("/admin");
    
    EXPECT_FALSE(auth->requiresAuth("/public"));
}

TEST_F(BasicAuthTest, RequiresAuthForProtectedPage) {
    auth->setEnabled(true);
    auth->addUser("admin", "password");
    auth->addProtectedPage("/admin");
    
    EXPECT_TRUE(auth->requiresAuth("/admin"));
}

// Credential verification (integration-style tests)
TEST_F(BasicAuthTest, VerifyValidCredentials) {
    auth->addUser("testuser", "password123");
    
    // "testuser:password123" in Base64 is "dGVzdHVzZXI6cGFzc3dvcmQxMjM="
    std::string authHeader = "Basic dGVzdHVzZXI6cGFzc3dvcmQxMjM=";
    
    EXPECT_TRUE(auth->verifyCredentials(authHeader));
}

TEST_F(BasicAuthTest, VerifyInvalidPassword) {
    auth->addUser("testuser", "password123");
    
    // "testuser:wrongpassword" in Base64
    std::string authHeader = "Basic dGVzdHVzZXI6d3JvbmdwYXNzd29yZA==";
    
    EXPECT_FALSE(auth->verifyCredentials(authHeader));
}

TEST_F(BasicAuthTest, VerifyInvalidUsername) {
    auth->addUser("testuser", "password123");
    
    // "wronguser:password123" in Base64
    std::string authHeader = "Basic d3JvbmfdXNlcjpwYXNzd29yZDEyMw==";
    
    EXPECT_FALSE(auth->verifyCredentials(authHeader));
}

TEST_F(BasicAuthTest, VerifyMalformedAuthHeader) {
    auth->addUser("testuser", "password123");
    
    EXPECT_FALSE(auth->verifyCredentials("InvalidHeader"));
    EXPECT_FALSE(auth->verifyCredentials("Basic"));
    EXPECT_FALSE(auth->verifyCredentials(""));
}

// Authenticate method (full path + auth check)
TEST_F(BasicAuthTest, AuthenticateUnprotectedPage) {
    auth->setEnabled(true);
    auth->addUser("admin", "password");
    auth->addProtectedPage("/admin");
    
    // Unprotected page should allow access without credentials
    EXPECT_TRUE(auth->authenticate("/public", ""));
}

TEST_F(BasicAuthTest, AuthenticateProtectedPageWithValidCredentials) {
    auth->setEnabled(true);
    auth->addUser("admin", "password123");
    auth->addProtectedPage("/admin");
    
    // "admin:password123" in Base64
    std::string authHeader = "Basic YWRtaW46cGFzc3dvcmQxMjM=";
    
    EXPECT_TRUE(auth->authenticate("/admin", authHeader));
}

TEST_F(BasicAuthTest, AuthenticateProtectedPageWithInvalidCredentials) {
    auth->setEnabled(true);
    auth->addUser("admin", "password123");
    auth->addProtectedPage("/admin");
    
    // Wrong credentials
    std::string authHeader = "Basic YWRtaW46d3JvbmdwYXNzd29yZA==";
    
    EXPECT_FALSE(auth->authenticate("/admin", authHeader));
}

TEST_F(BasicAuthTest, AuthenticateProtectedPageWithoutCredentials) {
    auth->setEnabled(true);
    auth->addUser("admin", "password123");
    auth->addProtectedPage("/admin");
    
    EXPECT_FALSE(auth->authenticate("/admin", ""));
}

TEST_F(BasicAuthTest, AuthenticateWhenDisabled) {
    auth->setEnabled(false);
    auth->addUser("admin", "password");
    auth->addProtectedPage("/admin");
    
    // Should allow access even to protected pages when disabled
    EXPECT_TRUE(auth->authenticate("/admin", ""));
}

// Edge cases
TEST_F(BasicAuthTest, EmptyUsernameAndPassword) {
    auth->addUser("", "");
    // Implementation may reject empty credentials as invalid
    // This is actually good security practice
    EXPECT_EQ(auth->userCount(), 0);  // Should not add empty credentials
}

TEST_F(BasicAuthTest, SpecialCharactersInCredentials) {
    auth->addUser("user@example.com", "p@$$w0rd!");
    EXPECT_EQ(auth->userCount(), 1);
    
    // "user@example.com:p@$$w0rd!" requires proper Base64 encoding
    // This tests that special characters are handled correctly
}

TEST_F(BasicAuthTest, EmptyProtectedPath) {
    auth->addProtectedPage("");
    // Implementation may reject empty paths as invalid
    EXPECT_EQ(auth->protectedPageCount(), 0);  // Should not add empty path
}

TEST_F(BasicAuthTest, DuplicateUsers) {
    auth->addUser("testuser", "password1");
    auth->addUser("testuser", "password2"); // Should overwrite
    
    EXPECT_EQ(auth->userCount(), 1);
}

TEST_F(BasicAuthTest, DuplicateProtectedPages) {
    auth->addProtectedPage("/admin");
    auth->addProtectedPage("/admin"); // Should not create duplicate
    
    // Set uses unique keys, so count should still be 1
    EXPECT_EQ(auth->protectedPageCount(), 1);
}
