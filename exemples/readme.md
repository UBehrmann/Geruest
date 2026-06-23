# Geruest Examples

Two entry points for adopters:

| Example | Purpose | Build target |
|---------|---------|--------------|
| **[minimal/](minimal/)** | First hour — ~40 lines, static site + one `/v1` route | `minimal` |
| **[showcase/](showcase/)** | Feature demo — routes, WebSocket chat, gates, Basic Auth, email | `showcase` |

Both use framework defaults: **merge off**, **obfuscation 0**, **WebP off** (unless you opt in via `.env` or code setters).

## Build

```bash
cd exemples
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Run from the build directory (website folders are copied next to each binary):

```bash
./minimal/minimal
./showcase/showcase
```

## Configuration

- **minimal** — optional `.env` beside the binary; `loadConfig()` picks up `PORT`, etc.
- **showcase** — copy `showcase/.env.example` to `build/showcase/.env` for SMTP and overrides.

To try the asset pipeline in showcase, set in `.env`:

```env
MERGE_ASSETS=true
WEBP_CONVERSION=true
```

Or uncomment the setter blocks in `showcase.cpp` (see comments there). Enable obfuscation only when you understand the JS pipeline — start with `setObfuscationLevel(0)`.

## When to use which

- **Starting a new project** → copy `minimal/` (or follow [Getting Started](../doc/GETTING_STARTED.md)).
- **Exploring framework features** → run `showcase` and read `showcase.cpp`.
