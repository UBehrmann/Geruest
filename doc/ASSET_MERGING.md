# Asset Merging

Automatically consolidate multiple CSS/JS files into single merged files per page, reducing HTTP requests and improving performance.

## Quick Start

```cpp
server.setMergeAssets(true);  // Enable (default: false)
server.addRoot("website");
server.init();
```

## How It Works

1. Scans HTML for CSS `<link>` and JS `<script>` tags
2. Extracts file paths and detects subdirectory structure
3. Merges content of all CSS files (separately for JS)
4. Saves to `/assets/css/[subdir]/pagename.css` and `/assets/js/[subdir]/pagename.js`
5. Replaces multiple tags with single references
6. **Always normalizes paths** (adds leading `/`, strips `/assets/css|js/` prefix)

**Input:**
```html
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="layout.css">
<script src="utils.js"></script>
<script src="main.js"></script>
```

**Output (Merging ON):**
```html
<link rel="stylesheet" href="/index.css">
<script src="/index.js"></script>
```

**Output (Merging OFF):**
```html
<link rel="stylesheet" href="/base.css">
<link rel="stylesheet" href="/layout.css">
<script src="/utils.js"></script>
<script src="/main.js"></script>
```

## Path Normalization

**Always runs** regardless of merging status. Critical for browser compatibility.

| Source                       | Normalized        | Reason                                                   |
| ---------------------------- | ----------------- | -------------------------------------------------------- |
| `style.css`                  | `/style.css`      | Adds leading `/` - prevents browser relative path issues |
| `/assets/css/base.css`       | `/base.css`       | Strips `/assets/css/` prefix                             |
| `sub/file.css`               | `/sub/file.css`   | Preserves directory structure                            |
| `/assets/css/a/b/c/file.css` | `/a/b/c/file.css` | Deep nesting preserved                                   |

**Why leading slash is critical:** Browsers compute relative paths CLIENT-SIDE before HTTP request:
```html
<!-- Page: /en/subfolder/page.html -->
<link href="base.css">   <!-- Browser requests: /en/subfolder/base.css ❌ (404) -->
<link href="/base.css">   <!-- Browser requests: /base.css ✓ -->
```

## Subdirectory Support

Unlimited nesting depth supported:
```
assets/css/
├── base.css                    # Root
├── sub/theme.css              # Level 2
└── sub/deep/advanced.css      # Level 3

# Merged files maintain structure:
/assets/css/pagename.css
/assets/css/sub/pagename.css
/assets/css/sub/deep/pagename.css
```

## Best Practices

**1. Load Order Matters:** Files merged in HTML order - load dependencies first
```html
<script src="jquery.js"></script>  <!-- First -->
<script src="app.js"></script>      <!-- After dependencies -->
```

**2. JavaScript Scope:** Files are directly concatenated - variables in global scope are shared across all files

**3. Flexible Path Format:** All valid:
```html
<link href="style.css">                   <!-- Recommended -->
<link href="/assets/css/style.css">       <!-- Also works -->
<link href="/style.css">                  <!-- Also works -->
```

Avoid: `../css/style.css` (relative paths with `../`)

**4. Test Both Modes:** Always test with merging enabled AND disabled

## Implementation

**Classes:**
- `AssetMerger`: Core merging logic (`src/builders/AssetMerger.cpp`)
- `HTMLBuilder`: Integration (`processAssetMerging()`, `ensureAbsoluteAssetPaths()`)

**Processing Order:**
1. Component inclusion
2. Translations
3. **If merging ON:** `processAssetMerging()` (merge + normalize paths)
4. **If merging OFF:** `ensureAbsoluteAssetPaths()` (normalize only)
5. Reference updates
6. File caching

**Regex Patterns:**
```cpp
std::regex(R"(<link[^>]*href\s*=\s*["']([^"']*\.css)["'][^>]*>)");  // CSS
std::regex(R"(<script[^>]* src\s*=\s*["']([^"']*\.js)["'][^>]*>)");  // JS
```

## Limitations

- No minification
- No source maps
- Static analysis only (build-time)
- No automatic scope isolation for JS
- No cache busting

## Troubleshooting

**Assets 404:** Verify files in `/assets/css|js/`, check paths start with leading `/`
**Merging Not Working:** Confirm `setMergeAssets(true)`, rebuild cleanly
**JS Errors:** Check dependency load order in HTML
**Nested Pages Fail:** Paths need leading `/` (framework adds automatically)