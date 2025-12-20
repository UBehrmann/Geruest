# Asset Merging Feature

## Overview

The Asset Merging feature automatically consolidates multiple CSS and JavaScript files referenced in an HTML page into single merged files per page. This reduces HTTP requests, improves page load times, and simplifies asset management without requiring manual configuration files.

## Table of Contents

- [Quick Start](#quick-start)
- [How It Works](#how-it-works)
- [Path Normalization](#path-normalization)
- [Subdirectory Support](#subdirectory-support)
- [Cross-Directory References](#cross-directory-references)
- [Examples](#examples)
- [Best Practices](#best-practices)
- [Technical Details](#technical-details)

---

## Quick Start

### Enabling Asset Merging

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    server.setPort(8080);
    
    // Enable asset merging (must be called before init/start)
    server.setMergeAssets(true);  // true = enable asset merging
    
    server.addRoot("website");
    server.init();
    server.start();
    
    return 0;
}
```

### Disabling Asset Merging

```cpp
// Disable asset merging (default behavior)
server.setMergeAssets(false);  // false = disable asset merging
// or simply don't call setMergeAssets() - merging disabled by default
```

---

## How It Works

### Processing Pipeline

1. **HTML Parsing**: The `AssetMerger` scans HTML files for CSS `<link>` and JS `<script>` tags
2. **File Extraction**: Extracts paths to all referenced asset files
3. **Subdirectory Detection**: Identifies common subdirectory structure using recursive path analysis
4. **Content Merging**: Concatenates the contents of all CSS files (and separately, all JS files)
5. **File Generation**: Saves merged files to `/assets/css/[subdir]/pagename.css` and `/assets/js/[subdir]/pagename.js`
6. **HTML Modification**: Replaces multiple asset tags with single references to merged files
7. **Path Normalization**: Strips `/assets/css/` and `/assets/js/` prefixes while preserving directory structure

### Source HTML (Input)

```html
<!-- Multiple CSS files (simplified paths - with or without leading slash) -->
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="layout.css">
<link rel="stylesheet" href="page-styles.css">

<!-- Multiple JS files -->
<script src="utils.js"></script>
<script src="api-client.js"></script>
<script src="main-app.js"></script>
```

**Note**: You can also use full paths like `href="/assets/css/base.css"` or paths without leading slash like `href="base.css"` - the framework handles both formats.

### Processed HTML (Output - Merging Enabled)

```html
<!-- Single merged CSS file -->
<link rel="stylesheet" href="/index.css">

<!-- Single merged JS file -->
<script src="/index.js"></script>
```

### Processed HTML (Output - Merging Disabled)

```html
<!-- Paths normalized but not merged -->
<link rel="stylesheet" href="/base.css">
<link rel="stylesheet" href="/layout.css">
<link rel="stylesheet" href="/page-styles.css">

<script src="/utils.js"></script>
<script src="/api-client.js"></script>
<script src="/main-app.js"></script>
```

---

## Path Normalization

**Path normalization runs regardless of whether merging is enabled or disabled.** This ensures compatibility with the server's routing system and prevents browser relative path resolution issues.

### Normalization Rules

| Source Path | Normalized Path | Description |
|------------|----------------|-------------|
| `base.css` | `/base.css` | Adds leading slash |
| `/assets/css/base.css` | `/base.css` | Root-level CSS |
| `/assets/js/utils.js` | `/utils.js` | Root-level JS |
| `subfolder/file.css` | `/subfolder/file.css` | Adds leading slash to nested |
| `/assets/css/subfolder/file.css` | `/subfolder/file.css` | Nested CSS |
| `/assets/js/subfolder/script.js` | `/subfolder/script.js` | Nested JS |
| `/assets/css/a/b/c/file.css` | `/a/b/c/file.css` | Deep nesting |

### Automatic Leading Slash Addition

**Critical for browser compatibility**: If asset paths don't have a leading `/`, browsers compute them as relative paths **client-side before the HTTP request reaches the server**. This causes nested pages to fail:

```html
<!-- From page: http://localhost:8080/en/subfolder/page.html -->
<link rel="stylesheet" href="base.css">  <!-- ❌ Browser requests: /en/subfolder/base.css (404) -->
<link rel="stylesheet" href="/base.css"> <!-- ✅ Browser requests: /base.css (works) -->
```

**The framework automatically adds leading slashes to all asset paths** to ensure they work from any page depth.

### Why Normalization?

1. **Browser Compatibility**: Leading `/` makes paths absolute so browsers don't compute relative paths
2. **Server Routing Compatibility**: Server expects paths without `/assets/` prefix
3. **Asset Merger Compatibility**: Merger needs full `/assets/css/` paths to locate files
4. **Cross-Platform**: Works consistently whether merging is enabled or disabled
5. **Flexibility**: Supports any directory nesting level

---

## Subdirectory Support

The asset merger supports **unlimited nesting depth** and automatically detects the common subdirectory structure for assets.

### Example Directory Structure

```
website/
├── assets/
│   ├── css/
│   │   ├── base.css                    # Root level
│   │   ├── layout.css
│   │   ├── subfolder/
│   │   │   ├── theme.css               # Level 2
│   │   │   └── components.css
│   │   └── subfolder/subsub/
│   │       ├── advanced.css            # Level 3
│   │       └── custom.css
│   └── js/
│       ├── utils.js                    # Root level
│       ├── subfolder/
│       │   └── manager.js              # Level 2
│       └── subfolder/subsub/
│           └── handler.js              # Level 3
└── html/
    ├── index.html
    └── subfolder/
        └── subsub/
            └── page.html
```

### Subdirectory Detection Algorithm

The asset merger uses **recursive path extraction** with `find_last_of('/')` to determine the full nested path:

```cpp
// Example: /assets/css/subfolder/subsub/file.css
// Extracts: "subfolder/subsub"
size_t afterCss = ref.href.find("/assets/css/");
size_t lastSlash = ref.href.find_last_of('/');
std::string subdir = ref.href.substr(afterCss + 12, lastSlash - (afterCss + 12));
```

### Merged File Placement

When merging is enabled, files are saved maintaining the subdirectory structure:

```
/assets/css/pagename.css                           # Root page
/assets/css/subfolder/pagename.css                # Level 2 page
/assets/css/subfolder/subsub/pagename.css         # Level 3 page
```

---

## Cross-Directory References

Pages can reference assets from **any directory level**, not just their own.

### Example: Nested Page Using Root Assets

```html
<!-- File: /html/subfolder/subsub/page.html -->
<!DOCTYPE html>
<html>
<head>
    <!-- Assets from nested directory -->
    <link rel="stylesheet" href="/assets/css/subfolder/subsub/custom.css">
    <script src="/assets/js/subfolder/subsub/handler.js"></script>
    
    <!-- Assets from root directory (cross-directory reference) -->
    <link rel="stylesheet" href="/assets/css/base.css">
    <script src="/assets/js/utils.js"></script>
</head>
<body>
    <!-- Page content -->
</body>
</html>
```

### After Processing (Merging Disabled)

```html
<!-- Paths normalized, all assets accessible -->
<link rel="stylesheet" href="/subfolder/subsub/custom.css">
<script src="/subfolder/subsub/handler.js"></script>

<!-- Root assets still accessible -->
<link rel="stylesheet" href="/base.css">
<script src="/utils.js"></script>
```

### After Processing (Merging Enabled)

**Note**: When merging is enabled, assets are merged **per page**, so cross-directory references become part of the merged file for that specific page.

---

## Examples

### Example 1: Simple Single-Level Page

**Source**: `/html/index.html`
```html
<link rel="stylesheet" href="/assets/css/base.css">
<link rel="stylesheet" href="/assets/css/layout.css">
<script src="/assets/js/utils.js"></script>
<script src="/assets/js/main.js"></script>
```

**Result (Merging Enabled)**:
- Generated: `/assets/css/index.css` (contains base.css + layout.css)
- Generated: `/assets/js/index.js` (contains utils.js + main.js)
- HTML Output:
  ```html
  <link rel="stylesheet" href="/index.css">
  <script src="/index.js"></script>
  ```

### Example 2: Nested Page with Subdirectory Assets

**Source**: `/html/admin/dashboard.html`
```html
<link rel="stylesheet" href="/assets/css/admin/dashboard-base.css">
<link rel="stylesheet" href="/assets/css/admin/dashboard-theme.css">
<script src="/assets/js/admin/dashboard-controller.js"></script>
<script src="/assets/js/admin/dashboard-api.js"></script>
```

**Result (Merging Enabled)**:
- Generated: `/assets/css/admin/dashboard.css`
- Generated: `/assets/js/admin/dashboard.js`
- HTML Output:
  ```html
  <link rel="stylesheet" href="/dashboard.css">
  <script src="/dashboard.js"></script>
  ```

### Example 3: Deep Nesting with Mixed References

**Source**: `/html/products/categories/electronics/smartphones.html`
```html
<!-- Category-specific assets -->
<link rel="stylesheet" href="/assets/css/products/categories/electronics/smartphones.css">
<script src="/assets/js/products/categories/electronics/product-viewer.js"></script>

<!-- Shared root assets -->
<link rel="stylesheet" href="/assets/css/base.css">
<script src="/assets/js/utils.js"></script>
```

**Result (Merging Enabled)**:
- Generated: `/assets/css/products/categories/electronics/smartphones.css` (merged)
- Generated: `/assets/js/products/categories/electronics/smartphones.js` (merged)
- All referenced assets (including root) merged into page-specific files

---

## Best Practices

### 1. Flexible Path Format

The framework supports multiple path formats - choose what's most convenient:

✅ **All Valid Formats**:
```html
<!-- Simplified (recommended for readability) -->
<link rel="stylesheet" href="style.css">
<link rel="stylesheet" href="subfolder/theme.css">

<!-- With /assets/ prefix (also supported) -->
<link rel="stylesheet" href="/assets/css/style.css">
<script src="/assets/js/script.js"></script>

<!-- With leading slash but no /assets/ -->
<link rel="stylesheet" href="/style.css">
```

**All formats are automatically normalized** - leading slashes are added if missing, and `/assets/css/` or `/assets/js/` prefixes are handled appropriately.

❌ **Avoid Relative Paths**:
```html
<link rel="stylesheet" href="../css/style.css"> <!-- Don't use ../ -->
```

### 2. Organize by Feature/Page

Structure your assets to match your HTML structure:

```
assets/
├── css/
│   ├── global/          # Shared across all pages
│   ├── home/            # Homepage-specific
│   ├── products/        # Product pages
│   └── admin/           # Admin section
└── js/
    ├── global/
    ├── home/
    ├── products/
    └── admin/
```

### 3. Load Order Matters

Asset merging preserves the order of files as they appear in the HTML. Ensure dependencies are loaded first:

```html
<!-- Dependencies first -->
<script src="/assets/js/jquery.js"></script>
<script src="/assets/js/bootstrap.js"></script>

<!-- Your code last -->
<script src="/assets/js/main.js"></script>
```

### 4. Development vs. Production

**Development** (merging disabled):
- Easier debugging with separate files
- See exactly which file has errors
- Faster reload during development

**Production** (merging enabled):
- Fewer HTTP requests
- Better performance
- Smaller total transfer size (if compression enabled)

### 5. Testing Both Modes

Always test your application with **both merging enabled and disabled** to ensure path compatibility:

```bash
# Test with merging disabled
./example  # If merging is off by default

# Test with merging enabled
# Modify code to enable merging, rebuild, test
```

---

## Technical Details

### Implementation Classes

#### `AssetMerger` Class
- **Location**: `src/builders/AssetMerger.cpp`
- **Purpose**: Core merging logic
- **Key Methods**:
  - `processHtml()`: Main entry point, processes HTML and returns merged content
  - `extractCssReferences()`: Finds all CSS `<link>` tags with positions
  - `extractJsReferences()`: Finds all JS `<script>` tags with positions
  - Uses regex patterns for reliable tag detection

#### `HTMLBuilder` Class
- **Location**: `src/builders/HTMLBuilder.cpp`
- **Purpose**: HTML processing pipeline integration
- **Key Methods**:
  - `processAssetMerging()`: Calls AssetMerger and saves merged files (includes leading slash addition)
  - `ensureAbsoluteAssetPaths()`: Adds leading `/` to all CSS/JS paths when merging is disabled
  - `buildHtml()`: Main build pipeline coordinator

### Processing Order

1. `replaceCurlyBrackets()` - Component inclusion
2. `replaceTranslations()` - Localization
3. **If merging enabled**: `processAssetMerging()` - Merge assets and add leading slashes
4. **If merging disabled**: `ensureAbsoluteAssetPaths()` - Add leading slashes to all asset paths
5. `replaceReferences()` - Add language prefixes (skips CSS/JS files)

### Data Structures

#### `MergeResult` Struct
```cpp
struct MergeResult {
    std::string modifiedHtml;    // HTML with merged references
    std::string mergedCss;       // Concatenated CSS content
    std::string mergedJs;        // Concatenated JS content
    bool hasCss;                 // Has CSS files to merge
    bool hasJs;                  // Has JS files to merge
    std::string cssSubdir;       // Detected CSS subdirectory
    std::string jsSubdir;        // Detected JS subdirectory
};
```

### Regex Patterns

**CSS Detection**:
```cpp
std::regex cssRegex(R"(<link[^>]*href\s*=\s*["']([^"']*\.css)["'][^>]*>)");
```

**JS Detection**:
```cpp
std::regex jsRegex(R"(<script[^>]*src\s*=\s*["']([^"']*\.js)["'][^>]*>)");
```

**Path Normalization**:
```cpp
std::regex cssNormalize(R"(href\s*=\s*["']/assets/css/([^"']+)["'])");
std::regex jsNormalize(R"(src\s*=\s*["']/assets/js/([^"']+)["'])");
```

### Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| HTML parsing | O(n) | Single-pass with regex |
| Subdirectory extraction | O(1) | `find_last_of('/')` |
| File merging | O(m) | m = total size of all assets |
| Path normalization | O(n) | Single-pass regex search |

---

## Browser Path Resolution Behavior

### Understanding Client-Side Path Computation

It's crucial to understand that **browsers compute relative paths CLIENT-SIDE before sending HTTP requests to the server**. This means:

#### The Problem
```html
<!-- Page URL: http://localhost:8080/en/subfolder/page.html -->
<link rel="stylesheet" href="base.css">  <!-- No leading slash -->
```

The browser computes the full URL **in the browser itself**:
- Current page: `/en/subfolder/page.html`
- Relative path: `base.css`
- Computed URL: `/en/subfolder/base.css` ⚠️
- HTTP request sent: `GET /en/subfolder/base.css` (404 - file doesn't exist)

**The server never sees the original "base.css" path** - by the time the request arrives, the browser has already computed it to `/en/subfolder/base.css`.

#### The Solution

Use absolute paths (with leading `/`) to tell the browser the path is relative to the domain root:

```html
<link rel="stylesheet" href="/base.css">  <!-- With leading slash -->
```

Now the browser knows it's absolute:
- Current page: `/en/subfolder/page.html` (doesn't matter)
- Absolute path: `/base.css`
- HTTP request sent: `GET /base.css` ✅

**The framework automatically adds leading slashes** to all asset paths during HTML processing, so you can write simple paths in your source files and they'll work correctly.

---

## Limitations and Considerations

### Current Limitations

1. **No Minification**: Merged files are not minified automatically
2. **No Source Maps**: Original file boundaries are not preserved
3. **Static Analysis**: Only processes files referenced in HTML at build time
4. **Comment Handling**: HTML comments are removed by default (configurable)

### Future Enhancements

Potential improvements for future versions:

- [ ] Optional minification/compression
- [ ] Source map generation
- [ ] Cache busting with file hashes
- [ ] Conditional merging based on file size thresholds
- [ ] Support for CSS `@import` directives
- [ ] Async/defer attribute preservation for JS

---

## Troubleshooting

### Assets Not Loading

**Problem**: Browser shows 404 errors for CSS/JS files

**Solutions**:
1. Verify paths start with `/assets/css/` or `/assets/js/`
2. Check that files exist in the `assets` directory
3. Ensure server is configured with correct root path
4. Check browser console for exact failed URLs

### Merging Not Working

**Problem**: Multiple asset files still appear in HTML output

**Solutions**:
1. Verify `mergeAssets` is set to `true` in `addRoot()`
2. Check that HTML uses correct path format
3. Ensure assets are referenced before components are included
4. Rebuild completely: `rm -rf build && cmake .. && make`

### Wrong File Order

**Problem**: JavaScript errors due to dependency loading order

**Solution**: Reorder `<script>` tags in source HTML to ensure dependencies load first

### Cross-Directory Issues

**Problem**: Nested pages can't find root-level assets

**Solution**: Use absolute paths starting with `/assets/` - normalization preserves directory structure

---

## Related Documentation

- [Installation Instructions](INSTALLATION_INSTRUCTIONS.md)
- [README](../README.md)
- [ContentBuilder Documentation](CONTENT_BUILDER.md) *(if exists)*

---

## Version History

- **v1.0** - Initial asset merging implementation
  - Basic CSS/JS merging
  - Subdirectory detection
  - Path normalization
  - Cross-directory reference support

---

**Last Updated**: December 2025  
**Author**: Geruest Framework Team
