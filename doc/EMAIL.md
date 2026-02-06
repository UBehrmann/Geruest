# Email System

Geruest includes a built-in email sending system with SMTP support, automatic retry logic, and spam protection features.

## Table of Contents

- [Overview](#overview)
- [Configuration](#configuration)
  - [Via .env File](#via-env-file)
  - [Via Environment Variables](#via-environment-variables)
  - [Via Code](#via-code)
- [Basic Usage](#basic-usage)
- [Spam Protection](#spam-protection)
- [Provider Setup](#provider-setup)
  - [Gmail](#gmail)
  - [SendGrid](#sendgrid)
  - [AWS SES](#aws-ses)
  - [Outlook/Office 365](#outlookoffice-365)
  - [Custom SMTP](#custom-smtp)
- [Advanced Features](#advanced-features)
- [Monitoring](#monitoring)
- [Troubleshooting](#troubleshooting)

---

## Overview

The `EmailSender` class provides:
- **SMTP Support**: Send emails via any SMTP server (ports 465, 587, 25)
- **Required TLS/SSL**: Enforced encryption by default - no silent fallback to plaintext
- **Queue System**: Asynchronous email sending with worker threads
- **Auto-Retry**: Failed emails are automatically retried up to 3 times
- **Spam Protection**: IP-based rate limiting and tracking
- **Thread-Safe**: Concurrent email sending without race conditions
- **Security Hardening**: Automatic SMTP header injection prevention

### System Requirements

**Required Dependencies:**
- **libcurl**: For SMTP communication
  - Linux: `sudo apt-get install libcurl4-openssl-dev` (Debian/Ubuntu)
  - macOS: `brew install curl`
  - Windows: Usually included with build tools

---

## Configuration

### Via .env File

Create a `.env` file in the same directory as your executable:

```env
# Email Configuration
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password
SMTP_FROM_ADDRESS=your-email@gmail.com

# Spam Protection (Optional)
EMAIL_MIN_INTERVAL=60              # Minimum seconds between emails from same IP
EMAIL_MAX_PER_IP=10               # Maximum emails per IP within tracking window
EMAIL_TRACKING_DURATION=3600      # How long to track IP activity (seconds)
EMAIL_MAX_QUEUE_SIZE=1000         # Maximum queued emails
```

Load in your application:

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    // Load all configuration from .env file
    server.loadConfig();
    
    // Email system is now configured and ready
    server.init();
    server.start();
    
    return 0;
}
```

### Via Environment Variables

Set environment variables (useful for Docker/containers):

```bash
# Linux/macOS
export SMTP_SERVER=smtp.gmail.com
export SMTP_PORT=587
export SMTP_USERNAME=your-email@gmail.com
export SMTP_PASSWORD=your-app-password
export EMAIL_FROM=your-email@gmail.com

# Windows (PowerShell)
$env:SMTP_SERVER="smtp.gmail.com"
$env:SMTP_PORT="587"
$env:SMTP_USERNAME="your-email@gmail.com"
$env:SMTP_PASSWORD="your-app-password"
$env:EMAIL_FROM="your-email@gmail.com"
```

Then call `loadConfig()` as shown above.

### Via Code

Configure email system programmatically:

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    // Initialize email system
    server.initEmail(
        "smtp.gmail.com",           // SMTP server
        587,                        // Port (587 for STARTTLS, 465 for SSL)
        "your-email@gmail.com",     // Username
        "your-app-password",        // Password
        "your-email@gmail.com"      // From address
    );
    
    // Optional: Configure spam protection
    server.setEmailMinInterval(60);          // Min seconds between emails/IP
    server.setEmailMaxPerIP(10);             // Max emails per IP
    server.setEmailTrackingDuration(3600);   // Track IPs for 1 hour
    server.setEmailMaxQueueSize(1000);       // Queue size limit
    
    server.init();
    server.start();
    
    return 0;
}
```

---

## Basic Usage

### Sending Emails from Routes

```cpp
server.addRoute("/api/contact", [](const geruest::HTTPRequest& req) {
    geruest::HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    
    // Parse request body (assuming JSON)
    std::string email = /* extract from req.getBody() */;
    std::string message = /* extract from req.getBody() */;
    
    // Queue the email (non-blocking)
    bool queued = geruest::EmailSender::getInstance().enqueueEmail(
        "admin@example.com",           // To
        "New Contact Form Submission",  // Subject
        "From: " + email + "\n\n" + message,  // Body
        req.getClientIP()              // Client IP (for spam protection)
    );
    
    if (queued) {
        response.setBody(R"({"success": true, "message": "Email sent"})");
    } else {
        response.setBody(R"({"success": false, "message": "Rate limit exceeded"})");
    }
    
    return response;
});
```

### HTML Emails

```cpp
std::string htmlBody = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; }
        .container { padding: 20px; background-color: #f5f5f5; }
        .button { 
            background-color: #4CAF50;
            color: white;
            padding: 10px 20px;
            text-decoration: none;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Welcome to Our Service!</h2>
        <p>Thank you for registering.</p>
        <a href="https://example.com/verify" class="button">Verify Email</a>
    </div>
</body>
</html>
)";

geruest::EmailSender::getInstance().enqueueEmail(
    "user@example.com",
    "Welcome!",
    "Content-Type: text/html; charset=UTF-8\r\n\r\n" + htmlBody,
    req.getClientIP()
);
```

### Complete Contact Form Example

```cpp
#include <Geruest.hpp>
#include <parser/JSONParser.hpp>
#include <email/EmailSender.hpp>

server.addRoute("/api/contact", [](const geruest::HTTPRequest& req) {
    geruest::HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");
    
    // Parse JSON request
    geruest::JSONParser parser(req.getBody());
    std::string name = parser.getString("name");
    std::string email = parser.getString("email");
    std::string message = parser.getString("message");
    
    // Validate input
    if (name.empty() || email.empty() || message.empty()) {
        response.setBody(R"({"success": false, "error": "Missing required fields"})");
        return response;
    }
    
    // IMPORTANT: Sanitize user input that goes into email headers
    // This prevents SMTP header injection attacks
    std::string safeName = geruest::EmailSender::sanitizeHeaderValue(name);
    std::string safeEmail = geruest::EmailSender::sanitizeHeaderValue(email);
    
    // Compose email body (can contain user input directly)
    std::string emailBody = 
        "New contact form submission:\n\n"
        "Name: " + name + "\n"  // Original values OK in body
        "Email: " + email + "\n"
        "Message:\n" + message;
    
    // Send email with sanitized header values
    bool sent = geruest::EmailSender::getInstance().enqueueEmail(
        "admin@example.com",
        "Contact Form: " + safeName,  // Use sanitized value in subject
        emailBody,
        req.getClientIP()
    );
    
    if (sent) {
        response.setBody(R"({"success": true, "message": "Message sent successfully"})");
    } else {
        response.setBody(R"({"success": false, "error": "Rate limit exceeded or queue full"})");
    }
    
    return response;
});
```

### Security: SMTP Header Injection Prevention

**⚠️ CRITICAL SECURITY WARNING**

The email system automatically sanitizes the `to` and `subject` parameters internally to prevent SMTP header injection attacks. However, for defense in depth, you should **explicitly sanitize any user input** before passing it to `enqueueEmail()`.

**Vulnerability Example (DO NOT USE):**
```cpp
// VULNERABLE CODE - DO NOT USE
std::string userName = parser.getString("name");  // Unsanitized user input

bool sent = EmailSender::getInstance().enqueueEmail(
    "admin@example.com",
    "Contact: " + userName,  // Attacker can inject \r\n to add headers!
    emailBody,
    req.getClientIP()
);
```

**Attack Scenario:**
An attacker sends `name: "John\r\nBcc: attacker@evil.com\r\nSubject: Stolen"`, resulting in:
```
Subject: Contact: John
Bcc: attacker@evil.com
Subject: Stolen
```

This allows attackers to:
- Add BCC/CC recipients to spy on emails
- Modify the From address
- Inject arbitrary SMTP headers
- Send emails to unintended recipients

**Secure Implementation:**
```cpp
// SECURE CODE - Use sanitizeHeaderValue()
std::string userName = parser.getString("name");
std::string safeUserName = geruest::EmailSender::sanitizeHeaderValue(userName);

bool sent = EmailSender::getInstance().enqueueEmail(
    "admin@example.com",
    "Contact: " + safeUserName,  // Safe from injection
    emailBody,
    req.getClientIP()
);
```

**What `sanitizeHeaderValue()` does:**
- Removes `\r` (carriage return)
- Removes `\n` (line feed)
- Removes other control characters (ASCII 0-31 except tab)
- Preserves readable content

**Best Practices:**
1. **Always sanitize** user input used in email subjects or recipient addresses
2. **Email body content** doesn't need sanitization (it's not part of SMTP headers)
3. **Validate email addresses** with regex before using them
4. **Use allowlists** for recipient addresses when possible
5. **Log suspicious input** that contains control characters

**Additional Validation Example:**
```cpp
#include <regex>

std::string validateEmail(const std::string& email) {
    static const std::regex emailPattern(
        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"
    );
    
    if (std::regex_match(email, emailPattern)) {
        return geruest::EmailSender::sanitizeHeaderValue(email);
    }
    return "";  // Invalid email
}

std::string recipientEmail = validateEmail(userInput);
if (recipientEmail.empty()) {
    // Reject invalid email
    response.setBody(R"({"error": "Invalid email address"})");
    return response;
}
```

---

## Spam Protection

The email system includes built-in spam protection:

### Rate Limiting

```cpp
// Limit emails from same IP
server.setEmailMinInterval(60);    // Minimum 60 seconds between emails
server.setEmailMaxPerIP(10);       // Maximum 10 emails per tracking window
server.setEmailTrackingDuration(3600);  // Track for 1 hour
```

**Example Scenario:**
- User submits contact form at 10:00:00
- User tries again at 10:00:30 → **REJECTED** (min interval not met)
- User tries again at 10:01:05 → **ACCEPTED**
- After 10 emails in the hour → **REJECTED** (max per IP reached)

### Queue Management

```cpp
server.setEmailMaxQueueSize(1000);  // Prevent queue overflow
```

If the queue is full, new emails are rejected to prevent memory issues.

### Monitoring Spam Protection

```cpp
auto& emailSender = geruest::EmailSender::getInstance();

// Get statistics
size_t sent = emailSender.getEmailsSent();
size_t rejected = emailSender.getEmailsRejected();
size_t queued = emailSender.getQueueSize();

std::cout << "Sent: " << sent 
          << ", Rejected: " << rejected 
          << ", Queued: " << queued << std::endl;
```

---

## Provider Setup

**🔒 TLS Security Note:** By default, TLS encryption is **required** for all SMTP connections. The email system will fail to send if TLS cannot be established, preventing credentials from being transmitted in cleartext. This is the secure default behavior.

### Gmail

**Requirements:**
- Enable 2-factor authentication
- Generate an App Password: https://myaccount.google.com/apppasswords

**Configuration:**
```env
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-16-char-app-password
EMAIL_FROM=your-email@gmail.com
```

**Important:**
- Use App Password, not your regular password
- Port 587 (STARTTLS) is recommended
- Port 465 (SSL/TLS) also works

### SendGrid

**Requirements:**
- Create SendGrid account
- Generate API Key with "Mail Send" permission

**Configuration:**
```env
SMTP_SERVER=smtp.sendgrid.net
SMTP_PORT=587
SMTP_USERNAME=apikey
SMTP_PASSWORD=your-sendgrid-api-key
EMAIL_FROM=verified-sender@yourdomain.com
```

**Important:**
- Username is literally "apikey"
- From address must be verified in SendGrid

### AWS SES

**Requirements:**
- AWS account with SES access
- Create SMTP credentials in SES console
- Verify sender email/domain

**Configuration:**
```env
SMTP_SERVER=email-smtp.us-east-1.amazonaws.com
SMTP_PORT=587
SMTP_USERNAME=your-smtp-username
SMTP_PASSWORD=your-smtp-password
EMAIL_FROM=verified@yourdomain.com
```

**Region-Specific Servers:**
- US East (N. Virginia): `email-smtp.us-east-1.amazonaws.com`
- US West (Oregon): `email-smtp.us-west-2.amazonaws.com`
- EU (Ireland): `email-smtp.eu-west-1.amazonaws.com`

### Outlook/Office 365

**Configuration:**
```env
SMTP_SERVER=smtp.office365.com
SMTP_PORT=587
SMTP_USERNAME=your-email@outlook.com
SMTP_PASSWORD=your-password
EMAIL_FROM=your-email@outlook.com
```

**For custom domains:**
```env
SMTP_SERVER=smtp.office365.com
SMTP_PORT=587
SMTP_USERNAME=you@yourdomain.com
SMTP_PASSWORD=your-password
EMAIL_FROM=you@yourdomain.com
```

### Custom SMTP

**Port Selection:**
- **Port 587** (STARTTLS): Most common, supports encryption upgrade
- **Port 465** (SSL/TLS): Legacy but still widely used
- **Port 25**: Unencrypted, often blocked by ISPs

```env
SMTP_SERVER=mail.yourdomain.com
SMTP_PORT=587
SMTP_USERNAME=user@yourdomain.com
SMTP_PASSWORD=your-password
EMAIL_FROM=noreply@yourdomain.com
```

---

## Advanced Features

### Auto-Retry on Failure

Emails that fail to send are automatically retried:

```cpp
// Automatic retry configuration (hardcoded)
// - Maximum 3 retry attempts
// - 5 second delay between retries
// - Errors logged to stderr
```

Failed emails after 3 attempts are logged but not queued again.

### Worker Thread Pool

```cpp
// Email system uses a pool of worker threads
// Defined in EmailSender.hpp:
constexpr size_t EMAIL_WORKER_COUNT = 3;  // 3 concurrent senders
constexpr int EMAIL_WORKER_IDLE_TIMEOUT_SECONDS = 10;  // Thread cleanup
```

### Custom Logging

The email system logs important events:

```cpp
// Successful sends (stdout)
[EmailSender] Email sent from sender@example.com to recipient@example.com

// Errors (stderr)
[EmailSender ERROR] Queue full, rejecting email from 192.168.1.1
[EmailSender ERROR] Failed to send email to user@example.com after 3 retries
[EmailSender ERROR] SMTP Error: Couldn't resolve host name (Server: smtp.example.com:587)
```

---

## Monitoring

### Statistics Endpoint Example

```cpp
server.addRoute("/api/email/stats", [](const geruest::HTTPRequest& req) {
    auto& emailSender = geruest::EmailSender::getInstance();
    
    std::string json = 
        R"({"emailsSent": )" + std::to_string(emailSender.getEmailsSent()) +
        R"(, "emailsRejected": )" + std::to_string(emailSender.getEmailsRejected()) +
        R"(, "queueSize": )" + std::to_string(emailSender.getQueueSize()) +
        R"(})";
    
    geruest::HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(json);
    return response;
});
```

### Health Check

```cpp
server.addRoute("/api/health", [](const geruest::HTTPRequest& req) {
    auto& emailSender = geruest::EmailSender::getInstance();
    size_t queueSize = emailSender.getQueueSize();
    
    bool healthy = queueSize < 500;  // Alert if queue grows large
    
    std::string status = healthy ? "healthy" : "degraded";
    std::string json = R"({"status": ")" + status + R"(", "queueSize": )" + 
                      std::to_string(queueSize) + R"(})";
    
    geruest::HTTPResponse response(healthy ? "200 OK" : "503 Service Unavailable");
    response.setHeader("Content-Type", "application/json");
    response.setBody(json);
    return response;
});
```

---

## Troubleshooting

### Common Issues

#### "EmailSender not initialized. Call init() first."

**Solution:** Initialize the email system before use:
```cpp
server.initEmail(/* ... */);  // Or use server.loadConfig()
```

#### SMTP Connection Errors

**Symptoms:**
```
[EmailSender ERROR] SMTP Error: Couldn't resolve host name
```

**Solutions:**
- Verify SMTP_SERVER is correct
- Check internet connectivity
- Ensure DNS resolution works

#### Authentication Failures

**Symptoms:**
```
[EmailSender ERROR] SMTP Error: Access denied, bad username or password
```

**Solutions:**
- **Gmail**: Use App Password, not regular password
- **SendGrid**: Username must be "apikey"
- **AWS SES**: Generate SMTP credentials (not IAM key/secret)
- Verify credentials don't have typos or extra whitespace

#### SSL/TLS Errors

**Symptoms:**
```
[EmailSender ERROR] SMTP Error: SSL certificate problem
[EmailSender ERROR] SMTP Error: SSL connect error
```

**Context:**
By default, `SMTP_USE_TLS=true` **requires** TLS encryption. The system will **not** fall back to plaintext if TLS fails. This is intentional security behavior to prevent credential disclosure.

**Solutions:**
- **Verify TLS support**: Ensure your SMTP server supports TLS/STARTTLS
- **Check certificates**: Ensure libcurl has valid CA certificates: `curl --version`
- **Port selection**:
  - Port 587: STARTTLS (starts plaintext, upgrades to TLS)
  - Port 465: Implicit TLS/SSL (encrypted from start)
- **Test TLS**: `openssl s_client -connect smtp.example.com:587 -starttls smtp`
- **Development only**: Set `SMTP_USE_TLS=false` for local testing (insecure!)

**Security Note:** If you see TLS errors, do **not** simply disable TLS. This would send your credentials and email content in cleartext. Instead, fix your TLS configuration or use a different SMTP provider.

#### Queue Full Errors

**Symptoms:**
```
[EmailSender ERROR] Queue full, rejecting email from 192.168.1.1
```

**Solutions:**
- Increase queue size: `server.setEmailMaxQueueSize(2000);`
- Check if SMTP server is slow/unreachable
- Monitor queue: `emailSender.getQueueSize()`

#### Rate Limit Issues

**Symptoms:** Emails rejected but no error logged

**Solutions:**
- Check spam protection settings are reasonable
- Use monitoring to track rejections: `getEmailsRejected()`
- Adjust limits for your use case:
```cpp
server.setEmailMinInterval(30);     // Lower minimum interval
server.setEmailMaxPerIP(20);        // Increase max per IP
```

### Testing SMTP Configuration

Use this route to test your email setup:

```cpp
server.addRoute("/api/test-email", [](const geruest::HTTPRequest& req) {
    bool sent = geruest::EmailSender::getInstance().enqueueEmail(
        "your-test-email@example.com",
        "Test Email from Geruest",
        "This is a test email to verify SMTP configuration.",
        "127.0.0.1"
    );
    
    geruest::HTTPResponse response("200 OK");
    response.setHeader("Content-Type", "application/json");
    response.setBody(sent ? R"({"success": true})" : R"({"success": false})");
    return response;
});
```

Visit `http://localhost:8080/api/test-email` and check your inbox.

---

## Security Best Practices

### 1. Never Commit Credentials

❌ **Don't:**
```cpp
server.initEmail("smtp.gmail.com", 587, "me@gmail.com", "MyPassword123", ...);
```

✅ **Do:**
```cpp
server.loadConfig();  // Load from .env file
```

### 2. Use .gitignore

```gitignore
# Add to .gitignore
.env
*.env
```

### 3. Use Environment Variables in Production

```bash
# Docker example
docker run -e SMTP_PASSWORD=$SMTP_PASSWORD myapp
```

### 4. Rotate Credentials Regularly

Change SMTP passwords/API keys periodically.

### 5. Require TLS Encryption

**Always use TLS** (default behavior):
```env
SMTP_USE_TLS=true  # Default - requires encrypted connection
```

**Never disable TLS in production:**
```env
# ❌ INSECURE - Only for local development/testing
SMTP_USE_TLS=false  
```

When `SMTP_USE_TLS=true`, the system **requires** TLS and will **fail** if encryption cannot be established. This prevents:
- Credential disclosure over cleartext SMTP
- Email content exposure
- Silent downgrade attacks where a network attacker forces plaintext communication

### 6. Enable Spam Protection

Always configure rate limiting:
```cpp
server.setEmailMinInterval(60);
server.setEmailMaxPerIP(10);
```

### 6. Sanitize User Input

**Always sanitize user input** used in email headers:
```cpp
// ALWAYS do this for safety
std::string safeName = EmailSender::sanitizeHeaderValue(userInput);
emailSender.enqueueEmail("admin@example.com", "Form: " + safeName, body, ip);
```

See the [Security: SMTP Header Injection Prevention](#security-smtp-header-injection-prevention) section above for detailed information about this critical security requirement.

---

## Next Steps

- [Configuration Guide](CONFIGURATION.md) - Complete .env and environment variable reference
- [Data Classes](DATA_CLASSES.md) - HTTPRequest and HTTPResponse details
- [Usage Guide](USAGE_GUIDE.md) - More server examples
- [Getting Started](GETTING_STARTED.md) - Basic setup

---

## API Reference

### EmailSender Methods

```cpp
// Singleton access
static EmailSender& getInstance();

// Queue an email (non-blocking, returns immediately)
bool enqueueEmail(const std::string& to, 
                  const std::string& subject,
                  const std::string& body, 
                  const std::string& clientIP);

// Security: Sanitize header values
static std::string sanitizeHeaderValue(const std::string& value);

// Configuration
void setMinEmailInterval(int seconds);
void setMaxEmailsPerIP(size_t count);
void setIPTrackingDuration(int seconds);
void setMaxQueueSize(size_t size);

// Monitoring
size_t getQueueSize() const;
size_t getEmailsSent() const;
size_t getEmailsRejected() const;

// Management
void clearIPTracking();  // Reset all IP tracking data
void stop();             // Stop workers and wait for queue to empty
```

### Geruest Email Methods

```cpp
// Initialize email system
void initEmail(const std::string& smtpServer,
               int smtpPort,
               const std::string& username,
               const std::string& password,
               const std::string& fromAddress);

// Configure spam protection
void setEmailMinInterval(int seconds);
void setEmailMaxPerIP(size_t count);
void setEmailTrackingDuration(int seconds);
void setEmailMaxQueueSize(size_t size);
```
