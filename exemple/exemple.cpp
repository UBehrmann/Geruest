/**
 * @file exemple.cpp
 * @date 11.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Example of how to use the Geruest server
 */

 #include <iostream>
#include <memory>
#include <string>
#include <csignal>
#include <filesystem>
#include "Geruest.hpp"
#if GERUEST_HAS_CURL
#include "email/EmailSender.hpp"
#endif

#define PORT 8080
#define HOSTNAME "localhost"

using namespace geruest;

// Use unique_ptr for automatic memory management
std::unique_ptr<Geruest> server;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    if (server) {
        server->stop();
    }
#if GERUEST_HAS_CURL
    // Stop email sender
    try {
        auto& emailSender = geruest::EmailSender::getInstance();
        emailSender.stop();
    } catch (...) {
        // Not initialized, ignore
    }
#endif
}

void addRoutes(Geruest* serverToAddRoutes);

int main(int argc, char* argv[]) {

    // Get the .env file path (same directory as executable)
    std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path exeDir = exePath.parent_path();
    std::filesystem::path envPath = exeDir / ".env";

    server = std::make_unique<Geruest>();

    // Set the signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ============================================================
    // SERVER CONFIGURATION - NEW FLEXIBLE APPROACH
    // ============================================================
    
    // Load configuration from .env file and environment variables
    // This loads settings like PORT, HOSTNAME, DEV_MODE, LOG_LEVEL, 
    // WEBP_CONVERSION, WEBP_QUALITY, MERGE_ASSETS, WORKER_THREADS,
    // and EMAIL (SMTP_SERVER, SMTP_USERNAME, etc.)
    //
    // Configuration Hierarchy: Code > .env > Environment Variables
    //
    // See doc/CONFIGURATION.md for full documentation
    // See .env.example for all available configuration keys
    //
    // Note: .env file should be in the same directory as the executable
    
    std::cout << "\n=== Loading Server Configuration ===" << std::endl;
    server->loadConfig(envPath.string());
    std::cout << "=====================================\n" << std::endl;
    
    // The loadConfig() method automatically initializes email sender if
    // SMTP credentials are provided in .env or environment variables.
    // You can also initialize email manually (overrides config):
    //
    // server->initEmail("smtp.gmail.com", 587, "user@gmail.com", 
    //                   "app-password", "noreply@example.com", true);
    // server->setEmailMinInterval(60);
    // server->setEmailMaxPerIP(10);
    // server->setEmailTrackingDuration(3600);
    // server->setEmailMaxQueueSize(1000);
    
    // Any explicit setter calls below will OVERRIDE config values
    // Uncomment these to override .env settings:
    
    // server->setPort(8080);                       // Override PORT from .env
    // server->setHostname("localhost");            // Override HOSTNAME from .env
    // server->enableDevMode();                     // Override DEV_MODE from .env
    // server->setLogLevel(LogLevel::Warning);      // Override LOG_LEVEL from .env
    // server->setWorkerThreadCount(16);            // Override WORKER_THREADS from .env
    // server->setMaxQueueSize(1000);               // Override MAX_QUEUE_SIZE from .env
    // server->setWebPConversion(true);             // Override WEBP_CONVERSION from .env
    // server->setWebPQuality(85);                  // Override WEBP_QUALITY from .env
    // server->setMergeAssets(true);                // Override MERGE_ASSETS from .env
    
    // ============================================================
    // LANGUAGE CONFIGURATION (optional, must be called before init/start)
    // ============================================================
    
    // Configure available languages - first language is the default
    // If not set, no language-based routing is performed
    std::vector<std::string> languages = {"en"};
    server->setAvailableLanguages(languages);
    
    std::cout << "=== Language Configuration ===" << std::endl;
    std::cout << "Available languages: en, de, fr" << std::endl;
    std::cout << "Default language: en" << std::endl;
    std::cout << "Language routing: ENABLED" << std::endl;
    std::cout << "================================\n" << std::endl;
    
    // To disable language routing, pass an empty vector:
    // server->setAvailableLanguages({});

    // ============================================================
    // ASSET MERGING CONFIGURATION (optional, must be called before init/start)
    // ============================================================
    
    // Enable automatic CSS/JS merging per page
    // When enabled, HTMLBuilder scans each page for <link> and <script> tags
    // and merges all local CSS/JS files into single bundled files.
    // This reduces the number of HTTP requests per page and eliminates the
    // need for manual JSON file mappings.
    //
    // Default: false (serves individual files as-is)
    //
    // Benefits when enabled:
    // - Single merged CSS file per page (e.g., index.css contains all CSS)
    // - Single merged JS file per page (e.g., index.js contains all JS)
    // - No manual configuration files needed
    // - Automatically updates when HTML templates change
    //
    // WARNING: JavaScript files are concatenated directly without scope isolation.
    // Ensure your JS files handle variable/function naming to avoid conflicts.
    
    server->setMergeAssets(true);  // ENABLED for testing
    
    std::cout << "=== Asset Merging Configuration ===" << std::endl;
    std::cout << "Asset merging: ENABLED" << std::endl;
    std::cout << "  ✓ CSS files will be merged per page" << std::endl;
    std::cout << "  ✓ JS files will be merged per page" << std::endl;
    std::cout << "  ✓ Merged files saved to /assets/css/ and /assets/js/" << std::endl;
    std::cout << "  ✓ HTML automatically updated with merged includes" << std::endl;
    std::cout << "===================================\n" << std::endl;

    // ============================================================
    // JAVASCRIPT OBFUSCATION CONFIGURATION (optional, must be called before init/start)
    // ============================================================
    
    // Enable JavaScript obfuscation to protect your code from casual analysis
    // When enabled, JavaScript files are obfuscated to make them harder to read
    // while maintaining functionality.
    //
    // Obfuscation Levels:
    // - Level 0: Disabled (default) - no obfuscation
    // - Level 1: Basic - variable/function name mangling + whitespace removal
    // - Level 2: Medium - Level 1 + string encoding + number obfuscation
    // - Level 3: Advanced - Level 2 + dead code + control flow obfuscation
    //
    // Default: 0 (disabled)
    //
    // Key Features:
    // - Automatic caching with configurable expiry (default: 7 days)
    // - Respects dev mode (automatically disables obfuscation for easier debugging)
    // - Works seamlessly with asset merging
    // - Excluded files are not obfuscated or merged
    //
    // Benefits:
    // - Protects intellectual property from casual copying
    // - Makes reverse engineering more difficult
    // - Deters script kiddies and automated tools
    //
    // IMPORTANT: Only applies when dev mode is OFF
    // Dev mode automatically disables obfuscation for easier debugging
    
    server->setObfuscationLevel(1);        // Enable medium obfuscation (recommended)
    server->setObfuscationCacheExpiry(7);  // Keep cached obfuscated files for 7 days
    
    // Exclude external libraries from obfuscation and merging
    // External libraries should be served as-is to avoid breaking them
    server->addObfuscationExclusion("jquery.min.js");
    server->addObfuscationExclusion("bootstrap.min.js");
    server->addObfuscationExclusion("lodash.js");
    
    std::cout << "=== JavaScript Obfuscation Configuration ===" << std::endl;
    std::cout << "Obfuscation level: 2 (Medium)" << std::endl;
    std::cout << "Cache expiry: 7 days" << std::endl;
    std::cout << "  ✓ Variables/functions will be mangled" << std::endl;
    std::cout << "  ✓ Whitespace removed (minification)" << std::endl;
    std::cout << "  ✓ Strings encoded with hex escapes" << std::endl;
    std::cout << "  ✓ Numbers obfuscated" << std::endl;
    std::cout << "  ✓ Excluded libraries: jquery.min.js, bootstrap.min.js, lodash.js" << std::endl;
    std::cout << "  ℹ Disabled in dev mode for easier debugging" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ============================================================
    // WEBP CONVERSION CONFIGURATION (optional, must be called before init/start)
    // ============================================================
    
    // Enable automatic PNG/JPG to WebP conversion
    // When enabled, HTMLBuilder scans each page for <img src="..."> tags
    // and CSS url() references with .png, .jpg, .jpeg extensions.
    // Images are converted to WebP format for smaller file sizes.
    //
    // Default: false (serves original images as-is)
    //
    // Behavior depends on mode:
    // - Dev mode: Images converted on-the-fly and cached in memory
    //             (never saved to disk, regenerated each restart)
    // - Production: Converted images saved to disk for efficiency
    //
    // Benefits:
    // - 25-35% smaller file sizes compared to PNG/JPG
    // - Faster page load times
    // - Automatic format optimization
    //
    // NOTE: Requires libwebp library for WebP encoding
    
    server->setWebPConversion(true);   // Enable WebP conversion
    server->setWebPQuality(80.0f);     // Set quality to 80% (default is 75%)
    
    std::cout << "=== WebP Conversion Configuration ===" << std::endl;
    std::cout << "WebP conversion: ENABLED" << std::endl;
    std::cout << "WebP quality: 80%" << std::endl;
    std::cout << "  ✓ PNG/JPG images will be converted to WebP" << std::endl;
    std::cout << "  ✓ HTML img tags automatically updated" << std::endl;
    std::cout << "  ✓ CSS url() references automatically updated" << std::endl;
    std::cout << "=====================================\n" << std::endl;

    // Add basic auth
    server->setBasicAuthEnabled(true);

    server->addBasicAuthUser("admin", "secret123");
    server->addProtectedPage("/devices/devices");
    
    // Get the absolute path to the website folder next to the executable
    std::filesystem::path executablePath = std::filesystem::canonical(std::filesystem::path(argv[0]).parent_path());
    std::filesystem::path websitePath = executablePath / "website";
    
    server->addRoot(websitePath.string());

    addRoutes(server.get());

    std::cout << "Starting Geruest server on port " << PORT << "..." << std::endl;
    std::cout << "Server will be accessible at http://" << HOSTNAME << ":" << PORT << std::endl;
    std::cout << "\n=== Test Routes ===" << std::endl;
    std::cout << "  GET  http://" << HOSTNAME << ":" << PORT << "/test" << std::endl;
    std::cout << "  GET  http://" << HOSTNAME << ":" << PORT << "/api/get" << std::endl;
    std::cout << "  POST http://" << HOSTNAME << ":" << PORT << "/api/post" << std::endl;
    std::cout << "  POST http://" << HOSTNAME << ":" << PORT << "/api/send-test-email (Email)" << std::endl;
    std::cout << "  WEB  http://" << HOSTNAME << ":" << PORT << "/email-test (Email Test Page)" << std::endl;
    std::cout << "  Wildcard: http://" << HOSTNAME << ":" << PORT << "/api/anything" << std::endl;
    std::cout << "  Static files from: ./website/" << std::endl;
    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "  Press Ctrl+C to stop the server gracefully" << std::endl;
    std::cout << "  Workers will finish current requests before shutdown" << std::endl;
    std::cout << "===================\n" << std::endl;

    server->init();

    server->start();

    // server is automatically cleaned up by unique_ptr
    
    return EXIT_SUCCESS;
}

void addRoutes(Geruest* serverToAddRoutes) {
    // Example GET route
    serverToAddRoutes->addRoute("/test", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/html");
        response.setBody("<h1>Hello, World!</h1><p>Welcome to Geruest server!</p>");
        return response;
    });

    // Wildcard route examples
    
    // Match any path under /api/
    serverToAddRoutes->addRoute("/api/*", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"wildcard": "api", "path": ")" + req.getPathString() + R"(", "message": "Caught by /api/* wildcard route"})");
        return response;
    });
    
    // Match specific pattern like /users/{id}/profile
    serverToAddRoutes->addRoute("/users/*/profile", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"wildcard": "user_profile", "path": ")" + req.getPathString() + R"(", "message": "User profile accessed"})");
        return response;
    });
    
    // Match files with specific extensions
    serverToAddRoutes->addRoute("/downloads/*.zip", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"wildcard": "zip_download", "path": ")" + req.getPathString() + R"(", "message": "ZIP file download requested"})");
        return response;
    });
    
    // Match multiple levels
    serverToAddRoutes->addRoute("/static/*/images/*", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"wildcard": "static_images", "path": ")" + req.getPathString() + R"(", "message": "Static image accessed"})");
        return response;
    });

#if GERUEST_HAS_CURL
    // ============================================================
    // EMAIL CONTACT FORM ENDPOINT
    // ============================================================
    
    // POST endpoint for contact form with email sending
    // Test with: curl -X POST http://localhost:8080/api/contact \
    //   -H "Content-Type: application/json" \
    //   -d '{"email":"user@example.com","name":"John Doe","message":"Hello!"}'
    serverToAddRoutes->addRoute("/api/contact", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setHeader("Access-Control-Allow-Origin", "*");
        
        try {
            // Parse JSON body
            geruest::JSONParser parser(req.getBody());
            
            std::string userEmail = parser.getString("email");
            std::string userName = parser.getString("name");
            std::string message = parser.getString("message");
            std::string clientIP = req.getClientIP();
            
            // Validate input
            if (userEmail.empty() || message.empty()) {
                response.setBody(R"({"status":"error","message":"Email and message are required"})");
                return response;
            }
            
            // IMPORTANT: Sanitize user input used in email headers to prevent SMTP injection
            std::string safeUserName = geruest::EmailSender::sanitizeHeaderValue(userName);
            std::string safeUserEmail = geruest::EmailSender::sanitizeHeaderValue(userEmail);
            
            // Prepare email content
            std::string emailSubject = "Contact Form: " + safeUserName;
            std::string emailBody = "New contact form submission:\n\n";
            emailBody += "From: " + userName + " (" + userEmail + ")\n";  // Original values OK in body
            emailBody += "IP: " + clientIP + "\n\n";
            emailBody += "Message:\n" + message + "\n";
            
            // Queue email
            auto& emailSender = geruest::EmailSender::getInstance();
            bool queued = emailSender.enqueueEmail(
                "admin@example.com",  // Replace with your actual admin email
                emailSubject,         // Using sanitized value
                emailBody,
                clientIP
            );
            
            if (queued) {
                response.setBody(R"({
                    "status":"success",
                    "message":"Your message has been sent successfully!",
                    "queue_size":)" + std::to_string(emailSender.getQueueSize()) + R"(,
                    "emails_sent":)" + std::to_string(emailSender.getEmailsSent()) + R"(
                })");
            } else {
                response.setBody(R"({
                    "status":"error",
                    "message":"Too many requests. Please try again later."
                })");
            }
            
        } catch (const std::exception& e) {
            response.setBody(R"({"status":"error","message":"Invalid request: )" + 
                           std::string(e.what()) + R"("})");
        }
        
        return response;
    });
#endif  // GERUEST_HAS_CURL

    // Specific exact routes that should take precedence over wildcards
    
    // GET endpoint (exact match takes precedence over /api/*)
    serverToAddRoutes->addRoute("/api/get", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody("{\"method\": \"GET\", \"message\": \"Exact GET endpoint (not wildcard)\", \"body\": \"" + req.getBody() + "\"}");
        return response;
    });

    // POST endpoint (exact match takes precedence over /api/*)
    serverToAddRoutes->addRoute("/api/post", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody("{\"method\": \"POST\", \"message\": \"Exact POST endpoint (not wildcard)\", \"body\": \"" + req.getBody() + "\"}");
        return response;
    });

    // PUT endpoint
    serverToAddRoutes->addRoute("/api/put", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody("{\"method\": \"PUT\", \"message\": \"PUT request received!\", \"body\": \"" + req.getBody() + "\"}");
        return response;
    });

    // DELETE endpoint
    serverToAddRoutes->addRoute("/api/delete", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setBody(R"({"method": "DELETE", "message": "DELETE request received!"})");
        return response;
    });

#if GERUEST_HAS_CURL
    // ============================================================
    // EMAIL TEST ENDPOINT
    // ============================================================
    
    // POST endpoint for sending simple test emails
    // Test with: curl -X POST http://localhost:8080/api/send-test-email \
    //   -H "Content-Type: application/json" \
    //   -d '{"to":"recipient@example.com"}'
    // Or visit: http://localhost:8080/email-test.html
    serverToAddRoutes->addRoute("/api/send-test-email", [](const HTTPRequest& req) {
        HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "application/json");
        response.setHeader("Access-Control-Allow-Origin", "*");
        
        try {
            // Parse JSON body
            geruest::JSONParser parser(req.getBody());
            
            std::string toEmail = parser.getString("to");
            std::string clientIP = req.getClientIP();
            
            // Validate input
            if (toEmail.empty()) {
                response.setBody(R"({"status":"error","message":"Recipient email address is required"})");
                return response;
            }
            
            // Sanitize email address for header safety
            std::string safeToEmail = geruest::EmailSender::sanitizeHeaderValue(toEmail);
            
            // Prepare test email content
            std::string emailSubject = "Test Email from Geruest Server";
            std::string emailBody = "This is a test email sent from your Geruest server.\n\n";
            emailBody += "Server: Geruest Framework\n";
            emailBody += "Timestamp: " + std::to_string(std::time(nullptr)) + "\n";
            emailBody += "Client IP: " + clientIP + "\n\n";
            emailBody += "If you received this email, your SMTP configuration is working correctly!\n";
            
            // Queue email
            auto& emailSender = geruest::EmailSender::getInstance();
            bool queued = emailSender.enqueueEmail(
                safeToEmail,  // Using sanitized value
                emailSubject,
                emailBody,
                clientIP
            );
            
            if (queued) {
                response.setBody(R"({
                    "status":"success",
                    "message":"Test email sent successfully to )" + toEmail + R"(!",
                    "queue_size":)" + std::to_string(emailSender.getQueueSize()) + R"(,
                    "emails_sent":)" + std::to_string(emailSender.getEmailsSent()) + R"(,
                    "emails_rejected":)" + std::to_string(emailSender.getEmailsRejected()) + R"(
                })");
            } else {
                response.setBody(R"({
                    "status":"error",
                    "message":"Rate limit exceeded. Please wait before sending another email."
                })");
            }
            
        } catch (const std::exception& e) {
            response.setBody(R"({"status":"error","message":"Invalid request: )" + 
                           std::string(e.what()) + R"("})");
        }
        
        return response;
    });
#endif  // GERUEST_HAS_CURL
}