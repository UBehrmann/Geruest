/**
 * @file BasicAuth.hpp
 * @date 07.12.2025
 *
 * @author Urs Behrmann
 *
 * @brief Basic HTTP Authentication system for protected pages
 * 
 * This module provides a simple Basic Authentication mechanism for protecting
 * specific pages. It is intended as a lightweight access control system,
 * not a full user management solution. Can be combined with application-level
 * user authentication for admin pages.
 */

#ifndef GERUEST_BASICAUTH_HPP
#define GERUEST_BASICAUTH_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace geruest {

/**
 * @class BasicAuth
 * @brief Manages Basic HTTP Authentication with user credentials and protected pages
 * 
 * Features:
 * - Toggle authentication on/off globally
 * - Manage username/password pairs
 * - Track which pages require authentication
 * - Verify credentials from Authorization header
 * - Automatic bypass when disabled or no users configured
 */
class BasicAuth {
private:
    // Authentication toggle - when false, all pages are accessible
    bool _enabled;
    
    // User credentials storage (username -> SHA-256 hashed password)
    std::unordered_map<std::string, std::string> _users;
    
    // Protected pages set (paths that require authentication)
    std::unordered_set<std::string> _protectedPages;
    
    /**
     * Decode Base64 encoded string
     * @param encoded Base64 encoded string
     * @return Decoded string
     */
    static std::string base64Decode(const std::string& encoded);
    
    /**
     * Compute SHA-256 hash of a string
     * @param input The string to hash
     * @return Hexadecimal representation of the hash
     */
    static std::string sha256Hash(const std::string& input);
    
    /**
     * Parse Authorization header to extract username and password
     * @param authHeader The Authorization header value
     * @param username Output parameter for username
     * @param password Output parameter for password
     * @return true if parsing successful
     */
    bool parseAuthorizationHeader(const std::string& authHeader, 
                                  std::string& username, 
                                  std::string& password) const;

public:
    /**
     * Constructor - authentication disabled by default
     */
    BasicAuth();
    
    /**
     * Enable or disable authentication globally
     * When disabled, all pages are accessible without credentials
     * @param enabled true to enable authentication
     */
    void setEnabled(bool enabled);
    
    /**
     * Check if authentication is enabled
     * @return true if authentication is active
     */
    bool isEnabled() const;
    
    /**
     * Add a user with credentials (password will be hashed with SHA-256)
     * @param username The username
     * @param password The password in plain text (will be hashed before storage)
     */
    void addUser(const std::string& username, const std::string& password);
    
    /**
     * Add a user with pre-hashed password
     * Use this if you want to store passwords that are already hashed
     * @param username The username
     * @param hashedPassword The SHA-256 hashed password (64 hex characters)
     */
    void addUserHashed(const std::string& username, const std::string& hashedPassword);
    
    /**
     * Helper function to generate a SHA-256 hash from a plain text password
     * Useful for generating hashes to store in configuration files
     * @param password Plain text password
     * @return SHA-256 hash as hexadecimal string
     */
    static std::string hashPassword(const std::string& password);
    
    /**
     * Remove a user
     * @param username The username to remove
     * @return true if user was removed, false if not found
     */
    bool removeUser(const std::string& username);
    
    /**
     * Check if any users are configured
     * @return true if at least one user exists
     */
    bool hasUsers() const;
    
    /**
     * Get number of configured users
     * @return User count
     */
    size_t userCount() const;
    
    /**
     * Clear all users
     */
    void clearUsers();
    
    /**
     * Add a page to protected pages list
     * @param path The path to protect (e.g., "/admin", "/api/admin")
     */
    void addProtectedPage(const std::string& path);
    
    /**
     * Remove a page from protected pages list
     * @param path The path to unprotect
     * @return true if page was removed, false if not found
     */
    bool removeProtectedPage(const std::string& path);
    
    /**
     * Check if a page is protected
     * @param path The path to check
     * @return true if page requires authentication
     */
    bool isPageProtected(const std::string& path) const;
    
    /**
     * Get number of protected pages
     * @return Protected page count
     */
    size_t protectedPageCount() const;
    
    /**
     * Clear all protected pages
     */
    void clearProtectedPages();
    
    /**
     * Verify credentials from Authorization header
     * @param authHeader The Authorization header value (e.g., "Basic dXNlcjpwYXNz")
     * @return true if credentials are valid
     */
    bool verifyCredentials(const std::string& authHeader) const;
    
    /**
     * Check if authentication is required for a given page
     * Returns false if:
     * - Authentication is disabled
     * - No users are configured
     * - Page is not in protected list
     * @param path The path to check
     * @return true if authentication check is required
     */
    bool requiresAuth(const std::string& path) const;
    
    /**
     * Authenticate a request for a specific path
     * @param path The requested path
     * @param authHeader The Authorization header value (empty if not present)
     * @return true if access is granted (either not required or valid credentials)
     */
    bool authenticate(const std::string& path, const std::string& authHeader) const;
};

}  // namespace geruest

#endif  // GERUEST_BASICAUTH_HPP
