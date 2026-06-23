# WebSockets

Geruest supports RFC 6455 WebSockets alongside existing HTTP routes. Register handlers with `addRouteWebSocket` — same path matching as `addRoute` (exact and `*` wildcards). An optional third argument adds an access gate (same types as gated API routes).

## Coroutine API (recommended)

Mirrors async `addRoute`: handler returns `boost::asio::awaitable<void>` and uses `co_await` on the connection.

```cpp
server.addRouteWebSocket("/chat", [](geruest::WebSocketConnection& ws,
                                     const geruest::HTTPRequest& req)
        -> boost::asio::awaitable<void> {
    co_await ws.send("welcome");

    while (ws.isOpen()) {
        geruest::WSMessage msg = co_await ws.recv();
        if (msg.isClose()) break;
        if (msg.isText()) co_await ws.send("echo: " + msg.text());
    }
    co_return;
});
```

## Callback API

For event-style handlers (`onOpen`, `onMessage`, `onClose`). Uses `sendNow()` for fire-and-forget replies from callbacks.

```cpp
geruest::WebSocketRoute chat;
chat.onOpen = [](geruest::WebSocketConnection& ws, const geruest::HTTPRequest&) {
    ws.sendNow("connected");
};
chat.onMessage = [](geruest::WebSocketConnection& ws, geruest::WSMessage msg) {
    if (msg.isText()) ws.sendNow(msg.text());
};
chat.onClose = [](geruest::WebSocketConnection&, uint16_t code, std::string_view reason) {
    // cleanup
};
server.addRouteWebSocket("/chat-cb", chat);
```

## Access gates (optional)

Pass a gate as the third argument to block the upgrade before the handshake completes. Uses the same `RouteGateHandler` / `AsyncRouteGateHandler` types as `addRoute(..., gate)`. Omit the third argument for ungated routes (backward compatible).

```cpp
// Sync gate: deny → 403 Forbidden (handler never runs)
server.addRouteWebSocket("/chat", chatHandler, [](const geruest::HTTPRequest& req) {
    return req.getHeader("authorization") == "Bearer mytoken";
});

// Async gate (e.g. DB/session lookup)
server.addRouteWebSocket("/chat", chatHandler, checkSessionAsync);

// Callback API + gate
server.addRouteWebSocket("/chat-cb", chat, gate);
```

Gate lookup uses the same path rules as HTTP route gates (exact/wildcard, longest match, language prefix). See [Gated Routes & Pages](GATED_ROUTES_PAGES.md).

## Configuration

```cpp
server.setWebSocketMaxMessageBytes(16 * 1024 * 1024);  // default 16 MiB
server.setWebSocketMaxFrameBytes(4 * 1024 * 1024);     // default 4 MiB
server.setWebSocketIdleTimeout(0);                     // 0 = disabled
server.setWebSocketPingInterval(0);                    // 0 = no server auto-ping
server.addWebSocketSubprotocol("chat");                // optional negotiation
```

## Behavior notes

- Upgrade detection runs before redirects and HTTP routes on `GET` requests with `Upgrade: websocket`.
- After a successful `101 Switching Protocols`, the TCP connection stays in WebSocket mode until the handler returns. HTTP keep-alive does not resume on that connection.
- Server automatically replies to client `ping` frames with `pong` (transparent to `recv()`).
- Invalid upgrade requests on WebSocket-looking paths receive `400`; unknown WebSocket paths receive `404`.
- Gated WebSocket routes: gate runs before the handshake; denial returns **403 Forbidden** (no upgrade).
- Use a reverse proxy (nginx, Caddy) for WSS/TLS termination.

## Client example (browser)

```javascript
const ws = new WebSocket("ws://localhost:8080/echo");
ws.onmessage = (e) => console.log(e.data);
ws.onopen = () => ws.send("hello");
```
