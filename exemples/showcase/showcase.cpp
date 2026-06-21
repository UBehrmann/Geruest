/**
 * @file showcase.cpp
 * @date 11.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Example of how to use the Geruest server
 */

#include <algorithm>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "Geruest.hpp"
#include "data/MethodNotAllowed.hpp"
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

namespace {

constexpr size_t kChatMaxNameLen  = 32;
constexpr size_t kChatMaxTextLen  = 2000;

struct ChatRoom {
    std::mutex                                        mu;
    std::vector<WebSocketConnection*>                 members;
    std::unordered_map<WebSocketConnection*, std::string> names;

    static std::string trimName(std::string name) {
        if (name.size() > kChatMaxNameLen) {
            name.resize(kChatMaxNameLen);
        }
        return name;
    }

    std::string nameFor(WebSocketConnection* ws) const {
        if (ws == nullptr) {
            return "?";
        }
        const auto it = names.find(ws);
        if (it != names.end() && !it->second.empty()) {
            return it->second;
        }
        return ws->clientIp();
    }

    void broadcastLocked(std::string_view line) {
        for (WebSocketConnection* peer : members) {
            if (peer != nullptr && peer->isOpen()) {
                peer->sendNow(line);
            }
        }
    }

    void join(WebSocketConnection& ws, const HTTPRequest& req) {
        std::string name = trimName(req.getParam("name"));
        if (name.empty()) {
            name = "guest-" + ws.clientIp();
        }

        std::lock_guard<std::mutex> lock(mu);
        names[&ws] = name;
        members.push_back(&ws);
        broadcastLocked("[join] " + name + " (" + std::to_string(members.size()) + " online)\n");
    }

    void leave(WebSocketConnection& ws) {
        std::string name;
        {
            std::lock_guard<std::mutex> lock(mu);
            name = nameFor(&ws);
            members.erase(std::remove(members.begin(), members.end(), &ws), members.end());
            names.erase(&ws);
            if (!members.empty()) {
                broadcastLocked("[leave] " + name + " (" + std::to_string(members.size()) + " online)\n");
            }
        }
    }

    void handleMessage(WebSocketConnection& ws, std::string_view text) {
        std::string payload(text);
        if (payload.size() > kChatMaxTextLen) {
            payload.resize(kChatMaxTextLen);
        }

        if (payload.rfind("/nick ", 0) == 0) {
            std::string newName = trimName(payload.substr(6));
            if (newName.empty()) {
                ws.sendNow("[system] usage: /nick YourName\n");
                return;
            }
            std::string oldName;
            {
                std::lock_guard<std::mutex> lock(mu);
                oldName = nameFor(&ws);
                names[&ws] = newName;
                broadcastLocked("[nick] " + oldName + " -> " + newName + "\n");
            }
            return;
        }

        std::string line;
        {
            std::lock_guard<std::mutex> lock(mu);
            line = nameFor(&ws) + ": " + payload + "\n";
            broadcastLocked(line);
        }
    }
};

ChatRoom g_chat;

}  // namespace

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
    
    // To disable language routing, pass an empty vector:
    // server->setAvailableLanguages({});

    // Asset pipeline (defaults: merge off, obfuscation 0, WebP off — same as loadConfig/.env).
    // Opt in via .env (MERGE_ASSETS, WEBP_CONVERSION) or uncomment:
    // server->setMergeAssets(true);
    // server->setObfuscationLevel(1);
    // server->setWebPConversion(true);

    // Add basic auth
    server->setBasicAuth(true);

    server->addBasicAuthUser("admin", "secret123");
    server->addProtectedPage("/devices/devices");

    // Page gate example: token or Bearer header (stacks with Basic Auth above)
    server->addGatedPage("/devices/devices", [](const HTTPRequest& req) {
        return req.getParam("token") == "demo"
            || req.getHeader("authorization") == "Bearer demo-token";
    });
    
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
    std::cout << "  WEB  http://" << HOSTNAME << ":" << PORT << "/chat (WebSocket Chat)" << std::endl;
    std::cout << "  WS   ws://" << HOSTNAME << ":" << PORT << "/chat?name=Alice" << std::endl;
    std::cout << "  Wildcard: http://" << HOSTNAME << ":" << PORT << "/api/anything" << std::endl;
    std::cout << "  Static files from: ./website/" << std::endl;
    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "  Press Ctrl+C to stop the server gracefully" << std::endl;
    std::cout << "  Workers will finish current requests before shutdown" << std::endl;
    std::cout << "===================\n" << std::endl;

    if (!server->init()) {
        return EXIT_FAILURE;
    }

    server->start();

    // server is automatically cleaned up by unique_ptr
    
    return EXIT_SUCCESS;
}

void addRoutes(Geruest* serverToAddRoutes) {
    // WebSocket echo (coroutine API)
    serverToAddRoutes->addRouteWebSocket(
        "/echo",
        [](WebSocketConnection& ws, const HTTPRequest&) -> boost::asio::awaitable<void> {
            WSMessage msg = co_await ws.recv();
            if (msg.isText()) {
                co_await ws.send(msg.text());
            }
            co_return;
        });

    // WebSocket echo (callback API)
    WebSocketRoute echoCb;
    echoCb.onMessage = [](WebSocketConnection& ws, WSMessage msg) {
        if (msg.isText()) {
            ws.sendNow(msg.text());
        }
    };
    serverToAddRoutes->addRouteWebSocket("/echo-cb", echoCb);

    WebSocketRoute chat;
    chat.onOpen = [](WebSocketConnection& ws, const HTTPRequest& req) { g_chat.join(ws, req); };
    chat.onMessage = [](WebSocketConnection& ws, WSMessage msg) {
        if (msg.isText()) {
            g_chat.handleMessage(ws, msg.text());
        }
    };
    chat.onClose = [](WebSocketConnection& ws, uint16_t, std::string_view) { g_chat.leave(ws); };
    serverToAddRoutes->addRouteWebSocket("/chat", chat);

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
        if (req.getMethod() != "GET" && req.getMethod() != "HEAD") {
            throw method_not_allowed("GET, HEAD");
        }
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