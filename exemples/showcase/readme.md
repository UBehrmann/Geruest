# Showcase Server

Full feature demo: wildcards, WebSocket chat, Basic Auth, page gates, optional email.

**Defaults:** merge off, obfuscation 0, WebP off (same as the framework). Opt in via `.env` or uncomment setters in `showcase.cpp`.

For a ~40-line starter, use [../minimal/](../minimal/).

## Build & run

```bash
cd exemples
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cp ../showcase/.env.example showcase/.env   # optional
./showcase/showcase
```

Open `http://localhost:8080` (or `PORT` from `.env`).

## Highlights

- Static site from `website/`
- REST examples under `/api/*`
- WebSocket chat at `/chat`
- Basic Auth + gated page at `/devices/devices` (`admin` / `secret123`, or `?token=demo`)
- Email test routes when built with libcurl and SMTP in `.env`

See [Configuration Guide](../../doc/CONFIGURATION.md) for all keys.
