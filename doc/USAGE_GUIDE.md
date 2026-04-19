# Usage Guide

Deploy and run Geruest servers.

## Local Development

**Linux:**
```bash
sudo apt-get install -y build-essential cmake git
cd Geruest && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j$(nproc) && sudo make install
```

**Windows (MSVC):** `cmake .. -A x64 && cmake --build . --config Release`  
**Windows (MinGW):** `cmake .. -G "MinGW Makefiles" && cmake --build .`  
**Visual Studio:** File → Open → CMake → Build (Ctrl+Shift+B)

## Docker

```dockerfile
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake git
WORKDIR /app
RUN git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && \
    mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) && make install

COPY . /app/myapp
WORKDIR /app/myapp
RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libstdc++6 curl && \
    groupadd -r appuser && useradd -r -g appuser appuser
WORKDIR /app
COPY --from=builder /app/myapp/build/myapp ./build/
COPY --from=builder /app/myapp/website ./website
RUN chown -R appuser:appuser /app
USER appuser
EXPOSE 8080
CMD ["./build/myapp"]
```

**WebP Support:** Add vcpkg before building:
```dockerfile
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh && /opt/vcpkg/vcpkg install libwebp
# Use -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
```

**docker-compose.yml:**
```yaml
services:
  webserver:
    build: .
    ports: ["8080:8080"]
    volumes: ["./website:/app/website:ro"]
    restart: unless-stopped
```

## Production

**Systemd (`/etc/systemd/system/app.service`):**
```ini
[Unit]
Description=Geruest App
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/opt/myapp
ExecStart=/opt/myapp/myapp
Restart=always
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=multi-user.target
```
```bash
sudo systemctl enable --now app && sudo journalctl -u app -f
```

**Nginx Reverse Proxy:**
```nginx
upstream backend { server 127.0.0.1:8080; keepalive 32; }

server {
    listen 80;
    server_name example.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name example.com;
    ssl_certificate /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;
    
    location / {
        proxy_pass http://backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
    
    location /assets/ {
        alias /opt/myapp/website/assets/;
        expires 30d;
    }
}
```

**Windows Service (NSSM):**
```powershell
nssm install AppName "C:\path\to\myapp.exe"
nssm set AppName AppDirectory "C:\path\to"
nssm start AppName
```

## Configuration

```cpp
server.setPort(8080);                         // Default: 8080
server.setHostname("0.0.0.0");                // Listen all interfaces
server.setWorkerThreadCount(16);              // Default: cores × 2 (io_context worker threads)
server.setMaxQueueSize(1000);                 // Default: 500 (max concurrent client sessions)
server.setAvailableLanguages({"en", "de"});   // First is default
server.setMergeAssets(true);
server.addRoot("/path/to/website");

// Environment vars
const char* port = std::getenv("PORT");
server.setPort(port ? std::atoi(port) : 8080);
```

**Threading profiles:** General (cores×2, max 500 sessions) | High-traffic (32 threads, 2000 sessions) | Low-resource (4 threads, 100 sessions)

## Platform Notes

**Linux:** Ports <1024 need root. Increase limits: `ulimit -n 65535`. TCP tuning: `/etc/sysctl.d/99-app.conf`
**Windows:** Allow port in firewall. `Ctrl+C` graceful shutdown.
**macOS:** Similar to Linux.
