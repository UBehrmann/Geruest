# HTML Component Injection

Geruest's HTML injection system allows you to include reusable components in your templates. This helps maintain consistency across pages and reduces code duplication.

## Table of Contents

- [Overview](#overview)
- [Syntax](#syntax)
- [Directory Structure](#directory-structure)
- [Examples](#examples)
- [Advanced Usage](#advanced-usage)
- [Best Practices](#best-practices)

---

## Overview

The component injection system works by:

1. Scanning HTML templates for `{component_path}` markers
2. Loading the referenced file content
3. Replacing the marker with the file content
4. Processing the result (translations, asset paths, etc.)

This happens at build time when pages are first requested, making subsequent requests fast.

---

## Syntax

### Basic Injection

Use curly braces with the path to the component file:

```
{relative/path/to/component.html}
```

The path is relative to the website root directory.

### Format

```html
<!-- In your template -->
{components/header.html}

<!-- Will be replaced with contents of /website/components/header.html -->
```

### Important Notes

- **No spaces** inside the curly braces
- **Path is relative** to the website root (same as `addRoot()`)
- **File extension** should be included
- **Nested injections** are supported (components can include other components)

---

## Directory Structure

### Recommended Layout

```
website/
├── components/
│   ├── header.html
│   ├── footer.html
│   ├── navigation.html
│   ├── sidebar.html
│   ├── cards/
│   │   ├── product-card.html
│   │   ├── user-card.html
│   │   └── feature-card.html
│   └── forms/
│       ├── login-form.html
│       ├── contact-form.html
│       └── search-form.html
├── html/
│   ├── index.html
│   ├── about.html
│   └── contact.html
└── assets/
    ├── css/
    └── js/
```

---

## Examples

### Basic Header/Footer Injection

#### Template (`html/index.html`)

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>My Website</title>
    <link rel="stylesheet" href="/assets/css/style.css">
</head>
<body>
    {components/header.html}
    
    <main>
        <h1>Welcome to My Website</h1>
        <p>This is the main content area.</p>
    </main>
    
    {components/footer.html}
</body>
</html>
```

#### Component (`components/header.html`)

```html
<header class="site-header">
    <div class="container">
        <div class="logo">
            <a href="/">
                <img src="/assets/images/logo.svg" alt="Logo">
            </a>
        </div>
        
        {components/navigation.html}
        
        <div class="header-actions">
            <button class="btn-menu" aria-label="Menu">☰</button>
        </div>
    </div>
</header>
```

#### Component (`components/navigation.html`)

```html
<nav class="main-nav">
    <ul>
        <li><a href="/">Home</a></li>
        <li><a href="/about">About</a></li>
        <li><a href="/services">Services</a></li>
        <li><a href="/contact">Contact</a></li>
    </ul>
</nav>
```

#### Component (`components/footer.html`)

```html
<footer class="site-footer">
    <div class="container">
        <div class="footer-grid">
            <div class="footer-section">
                <h4>About Us</h4>
                <p>Brief company description here.</p>
            </div>
            
            <div class="footer-section">
                <h4>Quick Links</h4>
                <ul>
                    <li><a href="/privacy">Privacy Policy</a></li>
                    <li><a href="/terms">Terms of Service</a></li>
                </ul>
            </div>
            
            <div class="footer-section">
                <h4>Contact</h4>
                <p>email@example.com</p>
            </div>
        </div>
        
        <div class="footer-bottom">
            <p>&copy; 2025 My Company. All rights reserved.</p>
        </div>
    </div>
</footer>
```

#### Generated Output

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>My Website</title>
    <link rel="stylesheet" href="/assets/css/style.css">
</head>
<body>
    <header class="site-header">
        <div class="container">
            <div class="logo">
                <a href="/">
                    <img src="/assets/images/logo.svg" alt="Logo">
                </a>
            </div>
            
            <nav class="main-nav">
                <ul>
                    <li><a href="/">Home</a></li>
                    <li><a href="/about">About</a></li>
                    <li><a href="/services">Services</a></li>
                    <li><a href="/contact">Contact</a></li>
                </ul>
            </nav>
            
            <div class="header-actions">
                <button class="btn-menu" aria-label="Menu">☰</button>
            </div>
        </div>
    </header>
    
    <main>
        <h1>Welcome to My Website</h1>
        <p>This is the main content area.</p>
    </main>
    
    <footer class="site-footer">
        <div class="container">
            <div class="footer-grid">
                <div class="footer-section">
                    <h4>About Us</h4>
                    <p>Brief company description here.</p>
                </div>
                
                <div class="footer-section">
                    <h4>Quick Links</h4>
                    <ul>
                        <li><a href="/privacy">Privacy Policy</a></li>
                        <li><a href="/terms">Terms of Service</a></li>
                    </ul>
                </div>
                
                <div class="footer-section">
                    <h4>Contact</h4>
                    <p>email@example.com</p>
                </div>
            </div>
            
            <div class="footer-bottom">
                <p>&copy; 2025 My Company. All rights reserved.</p>
            </div>
        </div>
    </footer>
</body>
</html>
```

### Nested Components

Components can include other components:

#### `components/header.html`

```html
<header>
    {components/logo.html}
    {components/navigation.html}
    {components/search-bar.html}
</header>
```

### Organized Component Folders

#### `components/cards/product-card.html`

```html
<div class="product-card">
    <div class="product-image">
        <img src="" alt="Product">
    </div>
    <div class="product-info">
        <h3 class="product-name"></h3>
        <p class="product-price"></p>
        <button class="btn-add-cart">Add to Cart</button>
    </div>
</div>
```

#### Using in template

```html
<section class="products">
    <h2>Featured Products</h2>
    <div class="product-grid">
        {components/cards/product-card.html}
        {components/cards/product-card.html}
        {components/cards/product-card.html}
    </div>
</section>
```

### Combining with Translations

Components work seamlessly with translations:

#### `components/header.html`

```html
<header>
    <nav>
        <a href="/">[assets/translations/nav.json:home]</a>
        <a href="/about">[assets/translations/nav.json:about]</a>
        <a href="/contact">[assets/translations/nav.json:contact]</a>
    </nav>
    <button>[assets/translations/nav.json:login]</button>
</header>
```

#### Usage in template

```html
<!DOCTYPE html>
<html>
<head>
    <title>[assets/translations/home.json:title]</title>
</head>
<body>
    {components/header.html}
    
    <main>
        <h1>[assets/translations/home.json:welcome]</h1>
    </main>
    
    {components/footer.html}
</body>
</html>
```

Both the template and component translations are processed!

---

## Advanced Usage

### Modal Components

```html
<!-- components/modals/confirm-modal.html -->
<div class="modal" id="confirmModal" role="dialog" aria-hidden="true">
    <div class="modal-backdrop"></div>
    <div class="modal-content">
        <header class="modal-header">
            <h2>Confirm Action</h2>
            <button class="modal-close" aria-label="Close">&times;</button>
        </header>
        <div class="modal-body">
            <p>Are you sure you want to proceed?</p>
        </div>
        <footer class="modal-footer">
            <button class="btn btn-secondary" data-dismiss="modal">Cancel</button>
            <button class="btn btn-primary" data-confirm="modal">Confirm</button>
        </footer>
    </div>
</div>
```

### Alert Components

```html
<!-- components/alerts/success-alert.html -->
<div class="alert alert-success" role="alert">
    <span class="alert-icon">✓</span>
    <span class="alert-message"></span>
    <button class="alert-close" aria-label="Dismiss">&times;</button>
</div>

<!-- components/alerts/error-alert.html -->
<div class="alert alert-error" role="alert">
    <span class="alert-icon">✕</span>
    <span class="alert-message"></span>
    <button class="alert-close" aria-label="Dismiss">&times;</button>
</div>
```

### Form Components

```html
<!-- components/forms/contact-form.html -->
<form class="contact-form" action="/api/contact" method="POST">
    <div class="form-group">
        <label for="name">[assets/translations/forms.json:name_label]</label>
        <input type="text" id="name" name="name" required 
               placeholder="[assets/translations/forms.json:name_placeholder]">
    </div>
    
    <div class="form-group">
        <label for="email">[assets/translations/forms.json:email_label]</label>
        <input type="email" id="email" name="email" required
               placeholder="[assets/translations/forms.json:email_placeholder]">
    </div>
    
    <div class="form-group">
        <label for="message">[assets/translations/forms.json:message_label]</label>
        <textarea id="message" name="message" rows="5" required
                  placeholder="[assets/translations/forms.json:message_placeholder]"></textarea>
    </div>
    
    <button type="submit" class="btn btn-primary">
        [assets/translations/forms.json:submit_button]
    </button>
</form>
```

### Layout Templates

Create a base layout and inject page-specific content:

```html
<!-- components/layouts/base.html -->
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    {components/head-common.html}
</head>
<body>
    {components/header.html}
    
    <main id="main-content">
        <!-- Page content injected here via server-side routing -->
    </main>
    
    {components/footer.html}
    {components/scripts-common.html}
</body>
</html>
```

---

## Best Practices

### 1. Keep Components Single-Purpose

```
✓ Good: header.html, navigation.html, footer.html
✗ Bad:  header-navigation-footer.html
```

### 2. Use Descriptive Names

```
✓ Good: product-card.html, user-avatar.html, search-form.html
✗ Bad:  card.html, avatar.html, form.html
```

### 3. Organize by Type

```
components/
├── layout/       # Header, footer, sidebar
├── navigation/   # Navbars, menus, breadcrumbs
├── cards/        # Product cards, user cards, etc.
├── forms/        # Login, contact, search forms
├── modals/       # Dialog boxes, popups
├── alerts/       # Notifications, messages
└── shared/       # Buttons, icons, badges
```

### 4. Avoid Deep Nesting

Limit component nesting to 2-3 levels for maintainability:

```
✓ Good: header.html → navigation.html → logo.html
✗ Bad:  page.html → section.html → card.html → button.html → icon.html
```

### 5. Include Necessary Styles

If a component has specific styles, document them:

```html
<!-- components/cards/feature-card.html -->
<!-- Required CSS: assets/css/components/feature-card.css -->
<div class="feature-card">
    ...
</div>
```

### 6. Make Components Self-Contained

Components should work without depending on external state:

```html
<!-- Good: All structure is in the component -->
<nav class="main-nav">
    <ul class="nav-list">
        <li class="nav-item"><a href="/">Home</a></li>
    </ul>
</nav>

<!-- Bad: Requires external wrapper -->
<li class="nav-item"><a href="/">Home</a></li>
```

### 7. Document Component Usage

Add comments at the top of complex components:

```html
<!-- 
  Component: User Profile Card
  Usage: {components/cards/user-profile.html}
  Requirements: 
    - CSS: assets/css/components/user-profile.css
    - JS: assets/js/components/user-profile.js
  Notes:
    - Requires user data to be populated via JavaScript
-->
<div class="user-profile-card">
    ...
</div>
```

---

## Processing Order

Understanding the processing order helps debug issues:

1. **Component Injection** - `{component}` markers are replaced first
2. **Translation Injection** - `[translation]` markers are processed second
3. **Asset Merging** - CSS/JS files are merged (if enabled)
4. **Reference Updates** - Links are updated with language prefixes
5. **File Saving** - Processed page is cached

This means translations inside components will be processed correctly!

---

## Limitations

### Not Supported Inside Script/Style Tags

Component markers inside `<script>` or `<style>` tags are ignored:

```html
<script>
    // This will NOT work
    const template = "{components/template.html}";
</script>
```

### No Dynamic Components

Components are static - you cannot pass parameters:

```html
<!-- This is NOT supported -->
{components/card.html?title=Hello}
```

For dynamic content, use JavaScript or server-side routes.

### File Must Exist

If the referenced file doesn't exist, the marker is left as-is or removed:

```html
<!-- If missing-component.html doesn't exist -->
{components/missing-component.html}
<!-- Results in empty string or unchanged marker -->
```

---

## Troubleshooting

### Component Not Being Injected

1. Check the file path is correct (relative to website root)
2. Verify the file exists
3. Ensure no spaces in the curly braces: `{path}` not `{ path }`

### Styles Not Applied

1. Make sure CSS is linked in the main template
2. Check class names match between component and CSS
3. Verify CSS file is in the correct location

### Nested Component Issues

1. Check for circular references (A includes B, B includes A)
2. Verify nested file paths are correct
3. Limit nesting depth

---

## Next Steps

- [Translations](TRANSLATIONS.md) - Multi-language support
- [Asset Merging](ASSET_MERGING.md) - CSS/JS optimization
- [Features](FEATURES.md) - All features overview
