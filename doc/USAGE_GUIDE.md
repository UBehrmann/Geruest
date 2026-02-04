# Usage Guide

This guide covers how to deploy and run Geruest servers in various environments.

## Table of Contents

- [Local Development](#local-development)
- [Docker Deployment](#docker-deployment)
- [Production Deployment](#production-deployment)
- [Platform-Specific Notes](#platform-specific-notes)
- [Configuration Options](#configuration-options)

---

## Local Development

### Linux

#### Quick Setup Script

```bash
#!/bin/bash
# setup.sh - Quick setup for Geruest development

# Install dependencies (Debian/Ubuntu)
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Clone and build
git clone https://github.com/yourusername/Geruest.git
cd Geruest
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$HOME/.local
make -j$(nproc)
sudo make install

# Build example
cd ../exemple
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
./exemple
```

#### Development Workflow

```bash
# 1. Make changes to source files

# 2. Rebuild library
cd /path/to/Geruest/build
make -j$(nproc)
make install

# 3. Rebuild your application
cd /path/to/your/app/build
make

# 4. Run with debugging
./yourapp

# Or with GDB
gdb ./yourapp
```

### Windows

#### Visual Studio Setup

1. **Install Prerequisites**:
   - Visual Studio 2019/2022 with C++ workload
   - CMake (included with Visual Studio or install separately)

2. **Open in Visual Studio**:
   - File → Open → CMake
   - Select the `CMakeLists.txt` in the Geruest root

3. **Build**:
   - Build → Build All
   - Or use the keyboard shortcut `Ctrl+Shift+B`

4. **Run**:
   - Select the target in the dropdown
   - Press `F5` to debug or `Ctrl+F5` to run

#### Command Line (MSVC)

```powershell
# Open Developer PowerShell for VS

# Build library
cd C:\path\to\Geruest
mkdir build && cd build
cmake .. -A x64
cmake --build . --config Release
cmake --install . --config Release

# Build your app
cd C:\path\to\your\app
mkdir build && cd build
cmake .. -A x64
cmake --build . --config Release
.\Release\yourapp.exe
```

#### Command Line (MinGW)

```powershell
# Using MSYS2 or MinGW terminal
cd /c/path/to/Geruest
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install .

# Build your app
cd /c/path/to/your/app
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
./yourapp.exe
```

---

## Docker Deployment

### Multi-Stage Dockerfile with WebP Support

This example shows a production-ready multi-stage build with vcpkg for WebP support:

```dockerfile
# Multi-stage Docker build for Geruest Application
# Stage 1: Build dependencies (Geruest library with WebP support)
FROM ubuntu:22.04 AS builder-deps

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    ca-certificates \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg for better dependency management
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh

# Install libwebp via vcpkg (provides proper CMake config)
RUN /opt/vcpkg/vcpkg install libwebp

# Create working directory
WORKDIR /app

# Clone and build Geruest library
RUN git clone https://github.com/UBehrmann/Geruest.git && \
    cd Geruest && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
          .. && \
    make -j$(nproc) && \
    make install

# Stage 2: Build your application
FROM ubuntu:22.04 AS builder-app

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy vcpkg and installed libraries from stage 1
COPY --from=builder-deps /opt/vcpkg /opt/vcpkg
COPY --from=builder-deps /usr/local /usr/local

# Create working directory
WORKDIR /app

# Copy your project files
COPY src/ ./src/
COPY CMakeLists.txt .
COPY website/ ./website/

# Build your application
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_PREFIX_PATH=/usr/local \
          -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
          .. && \
    make -j$(nproc)

# Stage 3: Runtime image (minimal size)
FROM ubuntu:22.04

# Install minimal runtime dependencies
RUN apt-get update && apt-get install -y \
    ca-certificates \
    libstdc++6 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN groupadd -r appuser && useradd -r -g appuser appuser

# Create application directory
WORKDIR /app

# Copy built application and website files
COPY --from=builder-app /app/build/myapp /app/build/
COPY --from=builder-app /app/website /app/website

# Note: Geruest is a static library with WebP statically linked
# All dependencies are already compiled into the binary, so no runtime libraries needed
# (If you build Geruest as shared, copy /usr/local/lib and run ldconfig)

# Change ownership to non-root user
RUN chown -R appuser:appuser /app

# Switch to non-root user
USER appuser

# Expose port (adjust as needed)
EXPOSE 8080

# Run the application
CMD ["./build/myapp"]
```

### Simple Dockerfile (Without WebP)

If you don't need WebP support, you can use this simpler version:

```dockerfile
# Simple multi-stage build without vcpkg
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Clone and build Geruest
RUN git clone https://github.com/UBehrmann/Geruest.git && \
    cd Geruest && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          .. && \
    make -j$(nproc) && \
    make install

# Copy and build your application
WORKDIR /app/myapp
COPY . .
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    curl \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r appuser && useradd -r -g appuser appuser

WORKDIR /app
COPY --from=builder /app/myapp/build/myapp ./build/
COPY --from=builder /app/myapp/website ./website

# Note: Geruest is statically linked - no runtime libraries needed

RUN chown -R appuser:appuser /app

USER appuser
EXPOSE 8080
CMD ["./build/myapp"]
```

### Multi-Stage with Alpine (Smaller Image)

**Note**: Alpine support requires additional configuration for WebP. Use Ubuntu-based images for best compatibility.

```dockerfile
# Dockerfile.alpine - Advanced users only
FROM alpine:3.18 AS builder

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    linux-headers

# Copy and build
WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install

# Build your application
WORKDIR /app/myapp
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make

# Runtime stage
FROM alpine:3.18

RUN apk add --no-cache libstdc++ libgcc curl

WORKDIR /app
COPY --from=builder /app/myapp/build/myapp .
COPY --from=builder /app/myapp/website ./website

EXPOSE 8080
CMD ["./myapp"]
```

### Build and Run Commands

```bash
# Build the image
docker build -t mygeruest-app .

# Run the container
docker run -d \
  --name myapp \
  -p 8080:8080 \
  -v $(pwd)/website:/app/website:ro \
  mygeruest-app

# View logs
docker logs -f myapp

# Stop and remove
docker stop myapp
docker rm myapp

# Rebuild and run (development)
docker build -t mygeruest-app . && \
docker stop myapp 2>/dev/null && docker rm myapp 2>/dev/null; \
docker run -d --name myapp -p 8080:8080 mygeruest-app
```

### Docker Compose

```yaml
# docker-compose.yml
version: '3.8'

services:
  webserver:
    build:
      context: .
      dockerfile: Dockerfile
    ports:
      - "8080:8080"
    volumes:
      # Mount website for live changes (development only)
      - ./website:/app/website:ro
    environment:
      - PORT=8080
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s

  # Optional: nginx reverse proxy
  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./certs:/etc/nginx/certs:ro
    depends_on:
      webserver:
        condition: service_healthy
    restart: unless-stopped
```

### Docker Compose Commands

```bash
# Build and start services
docker-compose up -d

# View logs
docker-compose logs -f webserver

# Rebuild after code changes
docker-compose up -d --build

# Stop services
docker-compose down

# Clean up (including volumes)
docker-compose down -v
```

### Development with Docker Volumes

For development, mount your source code:

```bash
docker run -it --rm \
  -v $(pwd):/app \
  -p 8080:8080 \
  -w /app \
  ubuntu:22.04 \
  bash -c "apt-get update && apt-get install -y build-essential cmake && \
           mkdir -p build && cd build && \
           cmake .. -DCMAKE_BUILD_TYPE=Debug && \
           make && ./exemple/exemple"
```

---

## Production Deployment

### Systemd Service (Linux)

Create `/etc/systemd/system/geruest-app.service`:

```ini
[Unit]
Description=Geruest Web Application
After=network.target

[Service]
Type=simple
User=www-data
Group=www-data
WorkingDirectory=/opt/myapp
ExecStart=/opt/myapp/myapp
Restart=always
RestartSec=5

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true

# Environment
Environment=PORT=8080

[Install]
WantedBy=multi-user.target
```

```bash
# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable geruest-app
sudo systemctl start geruest-app

# Check status
sudo systemctl status geruest-app

# View logs
sudo journalctl -u geruest-app -f
```

### Nginx Reverse Proxy

```nginx
# /etc/nginx/sites-available/myapp
upstream geruest_backend {
    server 127.0.0.1:8080;
    keepalive 32;
}

server {
    listen 80;
    server_name example.com;
    
    # Redirect HTTP to HTTPS
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name example.com;
    
    # SSL configuration
    ssl_certificate /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256;
    ssl_prefer_server_ciphers on;
    
    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    
    # Proxy to Geruest
    location / {
        proxy_pass http://geruest_backend;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header Connection "";
        
        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }
    
    # Static files (optional - serve directly from nginx)
    location /assets/ {
        alias /opt/myapp/website/assets/;
        expires 30d;
        add_header Cache-Control "public, immutable";
    }
}
```

### Windows Service

Use NSSM (Non-Sucking Service Manager):

```powershell
# Download NSSM from https://nssm.cc/download
# Extract to C:\nssm\

# Install as service
C:\nssm\win64\nssm.exe install GeruestApp

# In the GUI:
# - Path: C:\path\to\myapp.exe
# - Startup directory: C:\path\to\
# - Arguments: (leave empty)

# Or via command line
nssm install GeruestApp "C:\path\to\myapp.exe"
nssm set GeruestApp AppDirectory "C:\path\to"
nssm start GeruestApp

# Manage
nssm status GeruestApp
nssm stop GeruestApp
nssm remove GeruestApp
```

---

## Platform-Specific Notes

### Linux

- **Ports below 1024**: Require root or `CAP_NET_BIND_SERVICE` capability
- **File limits**: Increase with `ulimit -n 65535` for high-concurrency
- **TCP tuning**: Adjust sysctl settings for production

```bash
# /etc/sysctl.d/99-geruest.conf
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.ip_local_port_range = 1024 65535
```

### Windows

- **Firewall**: Allow inbound connections to your port
- **WinSock**: Automatically initialized by Geruest
- **Console**: `Ctrl+C` triggers graceful shutdown via signal handler

### macOS

- **Similar to Linux** for most operations
- **Code signing**: May be required for distribution
- **Firewall**: Allow in System Preferences → Security & Privacy

---

## Configuration Options

### Server Configuration

```cpp
Geruest server;

// Network settings
server.setPort(8080);              // Default: 8080
server.setHostname("0.0.0.0");     // Default: localhost
                                    // Use 0.0.0.0 to listen on all interfaces

// Thread pool settings
server.setWorkerThreadCount(16);   // Default: CPU cores × 2
server.setMaxQueueSize(1000);      // Default: 500

// Feature settings
server.setAvailableLanguages({"en", "de", "fr"});  // First is default
server.setMergeAssets(true);       // Default: false

// Authentication
server.setBasicAuthEnabled(true);
server.addBasicAuthUser("admin", "password");
server.addProtectedPage("/admin");

// Website root
server.addRoot("/path/to/website");
```

### Thread Pool Profiles

```cpp
unsigned int cpuCores = std::thread::hardware_concurrency();

// Conservative (general use)
server.setWorkerThreadCount(cpuCores * 2);
server.setMaxQueueSize(500);

// High-Traffic (production)
server.setWorkerThreadCount(32);
server.setMaxQueueSize(2000);

// Low-Resource (embedded/development)
server.setWorkerThreadCount(4);
server.setMaxQueueSize(100);
```

### Environment-Based Configuration

```cpp
#include <cstdlib>

int main() {
    Geruest server;
    
    // Read from environment variables
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 8080;
    server.setPort(port);
    
    const char* hostEnv = std::getenv("HOST");
    server.setHostname(hostEnv ? hostEnv : "localhost");
    
    // ... rest of configuration
}
```

---

## Next Steps

- [Features](FEATURES.md) - Detailed feature documentation
- [Data Classes](DATA_CLASSES.md) - Request/Response handling
- [Basic Authentication](BASIC_AUTH.md) - Secure your endpoints
- [Translations](TRANSLATIONS.md) - Multi-language support
