# Email System

Built-in SMTP email system with TLS encryption, queue-based async sending, auto-retry, and spam protection.

## Quick Start

**Dependencies:** libcurl (`sudo apt-get install libcurl4-openssl-dev`)

```cpp
// Method 1: Via .env file (recommended)
server.loadConfig();

// Method 2: Programmatic
server.initEmail("smtp.gmail.com", 587, "user@gmail.com", "app-password", "from@gmail.com");
```

**.env Configuration:**
```env
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password
SMTP_FROM_ADDRESS=your-email@gmail.com
EMAIL_MIN_INTERVAL=60              # Seconds between emails from same IP
EMAIL_MAX_PER_IP=10                # Max emails per IP per hour
EMAIL_TRACKING_DURATION=3600       # IP tracking window
EMAIL_MAX_QUEUE_SIZE=1000          # Queue limit
```

## Sending Emails

```cpp
// Basic email
geruest::EmailSender::getInstance().enqueueEmail(
    "recipient@example.com",
    "Subject",
    "Body content",
    req.getClientIP()  // For rate limiting
);

// HTML email
std::string html = "Content-Type: text/html; charset=UTF-8\r\n\r\n<h1>Hello</h1>";
EmailSender::getInstance().enqueueEmail("user@example.com", "Welcome", html, clientIP);

// Contact form with validation
geruest::JSONParser parser(req.getBody());
std::string name = parser.getString("name");
std::string email = parser.getString("email");
std::string message = parser.getString("message");

// CRITICAL: Sanitize header values to prevent SMTP injection
std::string safeName = EmailSender::sanitizeHeaderValue(name);
std::string safeEmail = EmailSender::sanitizeHeaderValue(email);

std::string body = "From: " + name + "\nEmail: " + email + "\n" + message;
bool sent = EmailSender::getInstance().enqueueEmail(
    "admin@example.com",
    "Contact: " + safeName,  // Use sanitized version
    body,                     // Body content doesn't need sanitization
    req.getClientIP()
);
```

## Security: SMTP Header Injection

**⚠️ CRITICAL:** Always sanitize user input in subjects/recipients to prevent header injection attacks.

```cpp
// VULNERABLE - attacker can inject "\r\nBcc: evil@hacker.com"
EmailSender::getInstance().enqueueEmail("admin@example.com", "Form: " + userInput, body, ip);

// SECURE - removes \r\n and control characters
std::string safe = EmailSender::sanitizeHeaderValue(userInput);
EmailSender::getInstance().enqueueEmail("admin@example.com", "Form: " + safe, body, ip);
```

**Additional validation:**
```cpp
std::regex emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
if (!std::regex_match(email, emailPattern)) { /* reject */ }
```

## Spam Protection

```cpp
server.setEmailMinInterval(60);           // Minimum seconds between emails/IP
server.setEmailMaxPerIP(10);              // Max emails per IP in tracking window
server.setEmailTrackingDuration(3600);    // Track IPs for N seconds
server.setEmailMaxQueueSize(1000);        // Prevent memory overflow

// Monitoring
auto& sender = EmailSender::getInstance();
size_t sent = sender.getEmailsSent();
size_t rejected = sender.getEmailsRejected();
size_t queued = sender.getQueueSize();
```

## Provider Setup

### Gmail
```env
SMTP_SERVER=smtp.gmail.com
SMTP_PORT=587
SMTP_USERNAME=your-email@gmail.com
SMTP_PASSWORD=your-app-password  # Generate at myaccount.google.com/apppasswords
EMAIL_FROM=your-email@gmail.com
```
**Note:** Requires 2FA enabled and App Password (not regular password).

### SendGrid
```env
SMTP_SERVER=smtp.sendgrid.net
SMTP_PORT=587
SMTP_USERNAME=apikey  # Literally "apikey"
SMTP_PASSWORD=your-sendgrid-api-key
EMAIL_FROM=verified@yourdomain.com  # Must be verified in SendGrid
```

### AWS SES
```env
SMTP_SERVER=email-smtp.us-east-1.amazonaws.com  # Region-specific
SMTP_PORT=587
SMTP_USERNAME=your-smtp-username  # SES console SMTP credentials
SMTP_PASSWORD=your-smtp-password
EMAIL_FROM=verified@yourdomain.com
```

### Outlook/Office 365
```env
SMTP_SERVER=smtp.office365.com
SMTP_PORT=587
SMTP_USERNAME=your-email@outlook.com
SMTP_PASSWORD=your-password
EMAIL_FROM=your-email@outlook.com
```

### Custom SMTP
```env
SMTP_SERVER=mail.yourdomain.com
SMTP_PORT=587  # 587=STARTTLS, 465=SSL/TLS, 25=plaintext (blocked by most ISPs)
SMTP_USERNAME=user@yourdomain.com
SMTP_PASSWORD=your-password
EMAIL_FROM=noreply@yourdomain.com
```

## Features

- **TLS Required:** `SMTP_USE_TLS=true` (default) enforces encryption, no plaintext fallback
- **Auto-Retry:** 3 attempts with 5-second delays
- **Thread Pool:** 3 concurrent worker threads
- **Queue System:** Non-blocking async sending
- **Monitoring:** Track sent/rejected/queued emails

## Troubleshooting

**"EmailSender not initialized"**
```cpp
server.initEmail(/* ... */);  // or server.loadConfig();
```

**Authentication Failed**
- Gmail: Use App Password (not regular password)
- SendGrid: Username must be "apikey"
- AWS SES: Use SMTP credentials (not IAM keys)

**TLS Errors**
```bash
# Test TLS: openssl s_client -connect smtp.gmail.com:587 -starttls smtp
# Verify curl: curl --version
```
Never disable TLS in production. Fix configuration instead.

**Rate Limit Rejections**
```cpp
server.setEmailMinInterval(30);    // Adjust as needed
server.setEmailMaxPerIP(20);
```

## API Reference

```cpp
// EmailSender (singleton)
static EmailSender& getInstance();
bool enqueueEmail(const std::string& to, const std::string& subject, 
                 const std::string& body, const std::string& clientIP);
static std::string sanitizeHeaderValue(const std::string& value);
size_t getQueueSize() const;
size_t getEmailsSent() const;
size_t getEmailsRejected() const;
void clearIPTracking();
void stop();

// Geruest methods
void initEmail(smtpServer, smtpPort, username, password, fromAddress);
void setEmailMinInterval(int seconds);
void setEmailMaxPerIP(size_t count);
void setEmailTrack ingDuration(int seconds);
void setEmailMaxQueueSize(size_t size);
```

## Security Best Practices

1. **Never commit credentials** - Use `.env` files (add to `.gitignore`)
2. **Always use TLS** - `SMTP_USE_TLS=true` (never disable in production)
3. **Sanitize input** - Use `sanitizeHeaderValue()` for all user input in headers
4. **Validate emails** - Use regex to verify email format
5. **Enable rate limiting** - Prevent abuse with `setEmailMinInterval()`/`setEmailMaxPerIP()`
6. **Rotate credentials** - Change passwords/keys regularly
7. **Use environment variables** - For Docker/containers
