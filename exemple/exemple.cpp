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
#include "email/EmailSender.hpp"
#include "EnvLoader.hpp"

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
    // Stop email sender
    try {
        auto& emailSender = geruest::EmailSender::getInstance();
        emailSender.stop();
    } catch (...) {
        // Not initialized, ignore
    }
}

void addRoutes(Geruest* serverToAddRoutes);

int main(int argc, char* argv[]) {

    // Load environment variables from .env file
    std::cout << "\n=== Environment Configuration ===" << std::endl;
    
    // Get the executable's directory
    std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path exeDir = exePath.parent_path();
    std::filesystem::path envPath = exeDir / ".." / ".env";
    std::filesystem::path normalizedEnvPath = std::filesystem::weakly_canonical(envPath);
    
    std::cout << "Looking for .env at: " << normalizedEnvPath << std::endl;
    
    if (!EnvLoader::load(normalizedEnvPath.string())) {
        std::cout << "No .env file found, using default values" << std::endl;
        std::cout << "Copy .env.example to .env and configure your SMTP settings" << std::endl;
    }
    // ============================================================
    // EMAIL SENDER CONFIGURATION (initialize before server)
    // ============================================================
    
    geruest::EmailSender::Config emailConfig;
    emailConfig.smtpServer = EnvLoader::get("SMTP_SERVER", "smtp.gmail.com");
    emailConfig.port = EnvLoader::getInt("SMTP_PORT", 587);
    emailConfig.username = EnvLoader::get("SMTP_USERNAME", "your-email@gmail.com");
    emailConfig.password = EnvLoader::get("SMTP_PASSWORD", "your-app-password");
    emailConfig.fromAddress = EnvLoader::get("SMTP_FROM_ADDRESS", "noreply@example.com");
    emailConfig.useTLS = true;
    
    // Initialize email sender
    geruest::EmailSender::init(emailConfig);
    auto& emailSender = geruest::EmailSender::getInstance();
    
    // Configure spam protection
    emailSender.setMinEmailInterval(EnvLoader::getInt("EMAIL_MIN_INTERVAL", 60));
    emailSender.setMaxEmailsPerIP(EnvLoader::getInt("EMAIL_MAX_PER_IP", 10));
    emailSender.setIPTrackingDuration(EnvLoader::getInt("EMAIL_TRACKING_DURATION", 3600));
    emailSender.setMaxQueueSize(EnvLoader::getInt("EMAIL_MAX_QUEUE_SIZE", 1000));
    
    std::cout << "\n=== Email Sender Configuration ===" << std::endl;
    std::cout << "SMTP Server: " << emailConfig.smtpServer << ":" << emailConfig.port << std::endl;
    std::cout << "Username: " << emailConfig.username << std::endl;
    std::cout << "Password: " << (emailConfig.password.empty() ? "[NOT SET]" : "[" + std::to_string(emailConfig.password.length()) + " chars]") << std::endl;
    std::cout << "From Address: " << emailConfig.fromAddress << std::endl;
    std::cout << "Spam Protection:" << std::endl;
    std::cout << "  Min interval: " << EnvLoader::getInt("EMAIL_MIN_INTERVAL", 60) << "s" << std::endl;
    std::cout << "  Max per IP: " << EnvLoader::getInt("EMAIL_MAX_PER_IP", 10) << " emails" << std::endl;
    std::cout << "  Tracking duration: " << EnvLoader::getInt("EMAIL_TRACKING_DURATION", 3600) << "s" << std::endl;
    std::cout << "===================================\n" << std::endl;

    std::cout << "=================================\n" << std::endl;

    server = std::make_unique<Geruest>();

    // Set the signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server->setPort(PORT);
    server->setHostname(HOSTNAME);
    
    // ============================================================
    // DEVELOPMENT MODE (optional, call before init/start)
    // ============================================================
    
    // Enable development mode for rapid iteration and debugging
    // When enabled:
    // - Log level automatically set to Debug (all logs shown)
    // - Files generated in-memory only (not saved to disk)
    // - Comments preserved in HTML/CSS/JS
    // - Asset merging still works if enabled separately
    //
    // Perfect for active development when files change frequently!
    // Files are regenerated on each request for immediate feedback.
    // DISABLE in production for better performance.
    
    // Uncomment to enable:
    // server->enableDevMode();
    
    // ============================================================
    // LOG LEVEL CONFIGURATION (can be changed anytime)
    // ============================================================
    
    // Configure log level to control verbosity
    // Levels: None < Error (framework default) < Warning < Info < Debug
    
    // Recommended for production/Docker to filter out timeout spam:
    server->setLogLevel(LogLevel::Warning);
    
    // Other options:
    // server->setLogLevel(LogLevel::None);     // Silent mode
    // server->setLogLevel(LogLevel::Error);    // Only errors (default if not set)
    // server->setLogLevel(LogLevel::Info);     // All normal logs (info and above)
    // server->setLogLevel(LogLevel::Debug);    // Verbose debugging
    
    std::cout << "\n=== Log Level Configuration ===" << std::endl;
    std::cout << "Log level: Warning (filters out timeout/connection noise)" << std::endl;
    std::cout << "  ✓ Errors: YES" << std::endl;
    std::cout << "  ✓ Warnings: YES" << std::endl;
    std::cout << "  ✗ Info messages: NO (filtered)" << std::endl;
    std::cout << "  ✗ Debug messages: NO (filtered)" << std::endl;
    std::cout << "================================\n" << std::endl;
    
    // ============================================================
    // THREAD POOL CONFIGURATION (must be called before init/start)
    // ============================================================
    
    // Get number of CPU cores
    unsigned int cpuCores = std::thread::hardware_concurrency();
    std::cout << "\n=== Thread Pool Configuration ===" << std::endl;
    std::cout << "Detected CPU cores: " << cpuCores << std::endl;
    
    // Choose configuration profile
    // Uncomment ONE of the following configurations:
    
    // PROFILE 1: Default/Conservative (recommended for general use)
    server->setWorkerThreadCount(cpuCores * 2);  // CPU cores × 2
    server->setMaxQueueSize(500);                // 500 pending connections
    std::cout << "Profile: CONSERVATIVE (default)" << std::endl;
    
    // PROFILE 2: High-Traffic (for production servers with heavy load)
    // server->setWorkerThreadCount(32);
    // server->setMaxQueueSize(2000);
    // std::cout << "Profile: HIGH-TRAFFIC" << std::endl;
    
    // PROFILE 3: Low-Resource (for embedded systems or development)
    // server->setWorkerThreadCount(4);
    // server->setMaxQueueSize(100);
    // std::cout << "Profile: LOW-RESOURCE" << std::endl;
    
    // PROFILE 4: Testing (small pool to easily test queue overflow)
    // server->setWorkerThreadCount(2);
    // server->setMaxQueueSize(5);
    // std::cout << "Profile: TESTING (small pool)" << std::endl;
    
    std::cout << "Worker threads: " << cpuCores * 2 << std::endl;
    std::cout << "Max queue size: 500" << std::endl;
    std::cout << "=================================\n" << std::endl;
    
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
            
            // Prepare email content
            std::string emailSubject = "Contact Form: " + userName;
            std::string emailBody = "New contact form submission:\n\n";
            emailBody += "From: " + userName + " (" + userEmail + ")\n";
            emailBody += "IP: " + clientIP + "\n\n";
            emailBody += "Message:\n" + message + "\n";
            
            // Queue email
            auto& emailSender = geruest::EmailSender::getInstance();
            bool queued = emailSender.enqueueEmail(
                "admin@example.com",  // Replace with your actual admin email
                emailSubject,
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
                toEmail,
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
}