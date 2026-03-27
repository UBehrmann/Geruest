# Redirects and 404

This guide explains how to configure redirects and custom 404 pages in Geruest.

## Custom 404 Page

Use `set404()` to configure a custom file that will be served when a page is not found.

```cpp
geruest::Geruest server;
server.addRoot("./website");

// Framework-relative path (resolved from your root structure)
server.set404("/404.html");

// or a path under your html folder, depending on your setup
// server.set404("/html/404.html");
```

Behavior:

- If the configured 404 file exists, it is served with `404 Not Found`.
- If it cannot be loaded, Geruest falls back to a default built-in 404 HTML response.

## Redirects

Geruest supports exact redirects, wildcard redirects, and bulk redirect maps.

### Exact Redirect

```cpp
// 301 by default
server.addRedirect("/old-about", "/en/about");

// Explicit temporary redirect
server.addRedirect("/old-contact", "/en/contact", 302);
```

### Wildcard Redirect

Use `*` in source and target to forward the matched segment.

```cpp
// /go/docs/getting-started  ->  /en/docs/getting-started
server.addRedirect("/go/*", "/en/*");
```

Notes:

- Wildcard forwarding uses the matched `*` segment from the source path.
- The most specific wildcard pattern wins when several match.

### Bulk Redirects (Map)

```cpp
server.addRedirects({
    {"/gh", "https://github.com/UBehrmann/Geruest"},
    {"/docs", "/en/documentation"},
    {"/short/*", "/en/pages/*"}
});
```

### External URLs

External targets are allowed. You can redirect to internal routes or full external URLs.

```cpp
server.addRedirect("/repo", "https://github.com/UBehrmann/Geruest");
```

## Priority Rules

Request handling priority is:

1. Exact redirect
2. Wildcard redirect
3. Exact route handler
4. Wildcard route handler
5. Static file resolution
6. 404 response

This makes short-link redirects predictable and keeps redirect behavior deterministic.

## Loop Protection

Geruest protects against redirect loops:

- Redirects that would create a loop are rejected.
- This applies to direct loops (`A -> A`) and chained loops (`A -> B -> A`).
- Wildcard redirects are also validated with loop checks.

If a redirect is rejected, server logs report it as invalid/loop-detected.

## Recommended URL Structure

Keep human-friendly URLs as redirect entries and map them to canonical routes:

```cpp
server.addRedirect("/start", "/en/getting-started");
server.addRedirect("/pricing", "/en/plans");
server.addRedirect("/contact", "/en/contact");
```

This keeps links short and stable while allowing internal page paths to evolve.