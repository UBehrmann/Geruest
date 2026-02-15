# JavaScript Obfuscation

Geruest provides built-in JavaScript obfuscation to make your code harder to analyze and reverse-engineer.

## 🎯 Overview

JavaScript obfuscation transforms your code to make it difficult to read while maintaining functionality. It works seamlessly with the asset merging system and respects development mode settings.

## 🔧 Configuration

### Basic Setup

```cpp
#include <geruest/Geruest.hpp>

int main() {
    Geruest server(8080);
    
    // Set obfuscation level (0=disabled, 1-3=increasing complexity)
    server.setObfuscationLevel(2);  // Medium obfuscation
    
    // Optional: Change cache expiry (default: 7 days)
    server.setObfuscationCacheExpiry(14);
    
    // Exclude external libraries from obfuscation
    server.addObfuscationExclusion("jquery.min.js");
    server.addObfuscationExclusion("bootstrap.min.js");
    server.addObfuscationExclusion("lodash.js");
    
    server.addRoot("./website");
    server.start();
}
```

## 📊 Obfuscation Levels

### Level 0: Disabled (Default)
- No obfuscation applied
- Files served as-is

### Level 1: Basic
- Variable and function name mangling
- Whitespace removal (minification)
- **Recommended for most production deployments**

### Level 2: Medium
- Everything from Level 1
- String literal encoding (hex escape sequences)
- Number obfuscation (hexadecimal, bitshift expressions)
- **Good balance between security and performance**

### Level 3: Advanced
- Everything from Level 2
- Dead code injection (unreachable code blocks)
- Control flow obfuscation
- **Maximum protection, larger file sizes**

## 🚫 Exclusion System

### Why Exclude Files?

Excluded files are:
- **NOT obfuscated** - served in original form
- **NOT merged** - served individually even with `setMergeAssets(true)`

Use exclusions for:
- External libraries (`jquery.min.js`, `moment.js`, etc.)
- Already minified/obfuscated code
- Third-party scripts that might break if modified
- Configuration files that need to remain readable

### How Exclusions Work

```cpp
// Exact filename matching
server.addObfuscationExclusion("analytics.js");
server.addObfuscationExclusion("gtag.js");
server.addObfuscationExclusion("config.js");
```

**Important:** Matching is exact - `"jquery.js"` != `"jquery.min.js"`

## 💾 Caching System

### How It Works

1. **First Request**: JS file is obfuscated and saved to disk
2. **Subsequent Requests**: Cached version is served (fast!)
3. **After Expiry**: File is re-obfuscated automatically

### Cache Location

Obfuscated files are saved **at the same path** as the original:
```
/assets/js/index.js  → Generated from page "index" → Cached on disk
```

### Cache Invalidation

Files are regenerated when:
- Cache expires (default: 7 days)
- Source file is modified (detected via file timestamp)
- Server restarts with different obfuscation level

### Manual Cache Clearing

Simply delete the generated JS files:
```bash
rm /path/to/website/assets/js/index.js
rm /path/to/website/assets/js/about.js
```

## 🔄 Integration with Asset Merging

Obfuscation works seamlessly with asset merging:

```cpp
server.setMergeAssets(true);        // Enable per-page CSS/JS merging
server.setObfuscationLevel(2);      // Enable obfuscation
server.addObfuscationExclusion("jquery.min.js");  // Exclude libraries

// Result:
// - Local JS files: merged into page.js, then obfuscated
// - External libraries: served separately, NOT obfuscated
// - Remote scripts (https://...): untouched
```

### Example HTML Processing

**Before:**
```html
<script src="https://cdn.example.com/lib.js"></script>
<script src="jquery.min.js"></script>  <!-- Excluded -->
<script src="utils.js"></script>
<script src="main.js"></script>
```

**After (with merging + obfuscation):**
```html
<script src="https://cdn.example.com/lib.js"></script>
<script src="jquery.min.js"></script>  <!-- Served separately, not obfuscated -->
<script src="index.js"></script>       <!-- utils.js + main.js merged & obfuscated -->
```

## 🛠️ Development Mode Behavior

```cpp
server.enableDevMode();
server.setObfuscationLevel(2);  // This is IGNORED in dev mode
```

**Dev mode automatically disables obfuscation** because:
- Faster iteration (no obfuscation overhead)
- Easier debugging (readable code in browser)
- No cache writes (files stay in memory)

## ⚡ Performance Considerations

### Build Time Impact

| Level | Impact | Typical Build Time |
|-------|--------|--------------------|
| 0     | None   | Instant |
| 1     | Low    | < 50ms per file |
| 2     | Medium | 50-100ms per file |
| 3     | High   | 100-200ms per file |

**Note:** Only first request is impacted - subsequent requests use cache!

### File Size Impact

| Level | Size Change | Example (10KB file) |
|-------|-------------|---------------------|
| 0     | 0%          | 10 KB |
| 1     | -40% to -60% (minification) | 4-6 KB |
| 2     | +10% to +30% (encoding overhead) | 11-13 KB |
| 3     | +30% to +50% (dead code) | 13-15 KB |

### Runtime Performance

Obfuscated code runs at **near-identical speed** to original:
- Name mangling: No impact
- String encoding: Negligible (decoded once)
- Number obfuscation: Negligible (constant expressions)
- Dead code: No impact (unreachable)

## 🔒 Security Notes

### What Obfuscation Provides

✅ **Raises the bar** for casual analysis  
✅ **Protects intellectual property** from quick copying  
✅ **Deters script kiddies** and automated tools  
✅ **Reduces code readability** significantly  

### What It Does NOT Provide

❌ **Encryption** - code is still JavaScript  
❌ **Complete protection** - determined analysts can still deobfuscate  
❌ **Security by obscurity** - don't rely on it for secret keys  

**Best Practice:** Obfuscation is a deterrent, not a security solution. Never embed secrets in client-side code.

## 📝 Complete Example

```cpp
#include <geruest/Geruest.hpp>

int main() {
    Geruest server(8080);
    
    // Production configuration
    server.setMergeAssets(true);           // Merge JS/CSS per page
    server.setObfuscationLevel(2);         // Medium obfuscation
    server.setObfuscationCacheExpiry(14);  // 2-week cache
    server.setRemoveComments(true);        // Strip comments
    
    // Exclude external libraries
    server.addObfuscationExclusion("jquery-3.6.0.min.js");
    server.addObfuscationExclusion("bootstrap.bundle.min.js");
    server.addObfuscationExclusion("chart.min.js");
    
    // Development override
    if (getenv("DEV_MODE") != nullptr) {
        server.enableDevMode();  // Disables obfuscation automatically
    }
    
    server.addRoot("./website");
    server.start();
    
    return 0;
}
```

## 🐛 Troubleshooting

### Problem: Obfuscated code breaks

**Solution:** Check if you're accidentally obfuscating a library that needs exact function names:
```cpp
server.addObfuscationExclusion("problematic-library.js");
```

### Problem: Cache not updating

**Solution:** Check file timestamps or manually delete cached files:
```bash
find /path/to/website/assets/js -name "*.js" -type f -delete
```

### Problem: Obfuscation not applying

**Checklist:**
1. Is obfuscation level > 0? Check `setObfuscationLevel()`
2. Is dev mode disabled? `enableDevMode()` disables obfuscation
3. Is file excluded? Check `addObfuscationExclusion()` calls
4. Check server logs for errors

### Problem: File size too large with level 3

**Solution:** Use level 2 for better size/protection balance:
```cpp
server.setObfuscationLevel(2);  // Good compromise
```

## 🎓 Best Practices

1. **Use Level 1 or 2 for production** - Level 3 rarely needed
2. **Combine with asset merging** - Reduces requests and protects code
3. **Always exclude external libraries** - Prevents breaking third-party code
4. **Test thoroughly after enabling** - Verify all functionality works
5. **Monitor file sizes** - Adjust level if files get too large
6. **Use appropriate cache expiry** - 7-14 days recommended
7. **Enable in production only** - Keep dev mode clean and fast

## 📚 Related Features

- [Asset Merging](ASSET_MERGING.md) - Combine CSS/JS files per page
- [Development Mode](DEV_MODE.md) - Fast iteration without caching
- [Configuration](CONFIGURATION.md) - Complete server configuration guide
