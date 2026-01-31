# Development Mode

## Overview
Development mode is designed to streamline the development process by providing verbose logging, disabling file caching, and keeping assets separate for easier debugging.

## Usage

```cpp
#include <geruest/Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    // Enable development mode before init() or start()
    server.enableDevMode();
    
    server.setPort(8080);
    server.addRoot("./website");
    server.init();
    server.start();
    
    return 0;
}
```

## What Development Mode Does

When `enableDevMode()` is called, the following changes are automatically applied:

### 1. **Verbose Logging** 📝
- Log level is automatically set to `LogLevel::Debug`
- All logs are shown, including:
  - Request details (method, path, IP)
  - Route matching information
  - File processing operations
  - Error details with full context

### 2. **No File Caching** 💾
- HTML, CSS, and JS files are generated in-memory only
- Files are **not saved to disk**
- Content is regenerated on every request
- Perfect for rapid iteration during development

### 3. **Asset Merging Preserved** 🔗
- Asset merging setting is **not changed** by dev mode
- If you have `setMergeAssets(true)`, merging still happens (but merged files aren't cached)
- If you have `setMergeAssets(false)`, assets are served individually
- You control asset merging independently of dev mode

### 4. **Comments Preserved** 💬
- HTML, CSS, and JS comments are kept in the output
- Makes debugging easier by preserving developer notes
- Useful for understanding template structure

## When to Use Development Mode

### ✅ **Use During Development**
- Active development with frequent HTML/CSS/JS changes
- Debugging template rendering issues
- Testing new features
- Local development environment

### ❌ **Disable in Production**
```cpp
// Production configuration - dev mode should NOT be enabled
geruest::Geruest server;
server.setPort(80);
server.setLogLevel(geruest::LogLevel::Warning);  // Only errors and warnings
server.setMergeAssets(true);                      // Enable for performance
server.addRoot("/var/www/html");
server.init();
server.start();
```

## Benefits

### For Development
- **Faster Iteration**: No need to clear cached files, changes are immediately reflected
- **Easier Debugging**: Verbose logs show exactly what's happening
- **Better Source Inspection**: Preserved comments make debugging easier
- **Flexible Asset Handling**: Asset merging can be on or off based on your testing needs
- **No File System Clutter**: Generated files don't pollute your working directory

### For Production
- **Better Performance**: File caching reduces CPU usage
- **Reduced Network Requests**: Merged assets mean fewer HTTP requests
- **Smaller Logs**: Only errors and warnings are logged
- **Optimized Output**: Comments removed, assets bundled

## Example Comparison

### Development Mode
```cpp
server.enableDevMode();comments preserved"
// Logs: [DEBUG] Processing /index.html for language: en
// Logs: [DEBUG] Loading component: header
// Logs: [DEBUG] Merging assets (if setMergeAssets is enabled)
// Files: Nothing saved to disk, all in-memory (regenerated per request) style.css, layout.css, theme.css
// Files: Nothing saved to disk, all in-memory
```

### Production Mode (Default)
```cpp
server.setLogLevel(geruest::LogLevel::Warning);
server.setMergeAssets(true);
// Logs: Minimal (only warnings/errors)
// Files: /website/assets/css/index.css (merged)
// Files: /website/assets/js/index.js (merged)
// Files: /website/html/index_en.html (cached)
```

## Configuration Options

Development mode is a convenience method that sets multiple options at once. You can also configure these individually:

```cpp
// Manual configuration (equivalent to enableDevMode())
server.setLogLevel(geruest::LogLevel::Debug);  // Show all logs
serverData.setRemoveComments(false);            // Keep comments (internal)
// Asset merging is controlled separately via setMergeAssets()
```

## Important Notes

⚠️ **Call Before Init**: `enableDevMode()` must be called before `init()` or `start()`

⚠️ **Not Thread-Safe for Toggle**: Once the server is running, dev mode cannot be toggled

⚠️ **Performance Impact**: Dev mode has higher CPU usage due to regenerating content on every request

⚠️ **Production Default**: Development mode is **disabled by default** for production safety

## Troubleshooting

### Changes Not Appearing?
- Ensure `enableDevMode()` is called before `init()`
- Check that you're editing the correct source files (not cached versions)
- Verify the server has read access to your website directory

### Too Much Logging?
```cpp
// Reduce log verbosity while keeping other dev features
server.enableDevMode();
server.setLogLevel(geruest::LogLevel::Info);  // Override to Info level
```

### Need File Caching in Dev?
Dev mode is all-or-nothing for caching. For partial dev features, configure manually:
```cpp
server.setLogLevel(geruest::LogLevel::Debug);  // Verbose logs only
// File caching enabled (default), dev mode not enabled
```

### Want Individual Assets in Dev?
```cpp
server.enableDevMode();           // Enable dev mode
server.setMergeAssets(false);     // Disable asset merging
// Now you have: verbose logs + no caching + individual assets
```

## See Also
- [GETTING_STARTED.md](GETTING_STARTED.md) - Basic server setup
- [USAGE_GUIDE.md](USAGE_GUIDE.md) - Complete configuration guide
- [FEATURES.md](FEATURES.md) - All framework features
