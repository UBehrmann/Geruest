# Translation System

Geruest provides a powerful multi-language system that allows you to create translated websites without duplicating HTML files. This document covers how to set up and use the translation injection feature.

## Table of Contents

- [Overview](#overview)
- [Setup](#setup)
- [Translation Files](#translation-files)
- [Syntax](#syntax)
- [Examples](#examples)
- [Language Routing](#language-routing)
- [Best Practices](#best-practices)

---

## Overview

The translation system works by:

1. **Defining available languages** in your server configuration
2. **Creating JSON translation files** with key-value pairs for each language
3. **Using injection syntax** in HTML templates to reference translations
4. **Automatic processing** by the HTMLBuilder at build time

When a page is requested, the HTMLBuilder:
- Identifies the requested language from the URL
- Replaces translation markers with the appropriate text
- Saves the processed page for that language

---

## Setup

### Server Configuration

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;
    
    // Set available languages
    // First language is the default
    server.setAvailableLanguages({"en", "de", "fr", "es"});
    
    server.setPort(8080);
    server.addRoot("/path/to/website");
    server.init();
    server.start();
    
    return 0;
}
```

### Directory Structure

```
website/
├── assets/
│   └── translations/
│       ├── navigation.json    # Navigation translations
│       ├── home.json          # Home page translations
│       ├── about.json         # About page translations
│       └── errors.json        # Error messages
├── components/
│   └── header.html
└── html/
    ├── index.html             # Template
    ├── about.html             # Template
    ├── en/                    # Generated
    │   ├── index.html
    │   └── about.html
    ├── de/                    # Generated
    │   ├── index.html
    │   └── about.html
    └── fr/                    # Generated
        ├── index.html
        └── about.html
```

---

## Translation Files

Translation files are JSON files with language codes as top-level keys.

### Basic Structure

```json
{
    "en": {
        "key": "English text"
    },
    "de": {
        "key": "German text"
    },
    "fr": {
        "key": "French text"
    }
}
```

### Example: `navigation.json`

```json
{
    "en": {
        "home": "Home",
        "about": "About Us",
        "services": "Services",
        "contact": "Contact",
        "login": "Log In",
        "logout": "Log Out",
        "search_placeholder": "Search..."
    },
    "de": {
        "home": "Startseite",
        "about": "Über Uns",
        "services": "Dienstleistungen",
        "contact": "Kontakt",
        "login": "Anmelden",
        "logout": "Abmelden",
        "search_placeholder": "Suchen..."
    },
    "fr": {
        "home": "Accueil",
        "about": "À Propos",
        "services": "Services",
        "contact": "Contact",
        "login": "Connexion",
        "logout": "Déconnexion",
        "search_placeholder": "Rechercher..."
    }
}
```

### Example: `home.json`

```json
{
    "en": {
        "title": "Welcome to Our Website",
        "subtitle": "Building the Future Together",
        "intro": "We provide innovative solutions for modern businesses.",
        "learn_more": "Learn More",
        "get_started": "Get Started",
        "features_title": "Our Features",
        "feature_1_title": "Fast",
        "feature_1_desc": "Lightning-fast performance",
        "feature_2_title": "Secure",
        "feature_2_desc": "Enterprise-grade security",
        "feature_3_title": "Scalable",
        "feature_3_desc": "Grows with your business"
    },
    "de": {
        "title": "Willkommen auf unserer Website",
        "subtitle": "Gemeinsam die Zukunft gestalten",
        "intro": "Wir bieten innovative Lösungen für moderne Unternehmen.",
        "learn_more": "Mehr erfahren",
        "get_started": "Loslegen",
        "features_title": "Unsere Funktionen",
        "feature_1_title": "Schnell",
        "feature_1_desc": "Blitzschnelle Leistung",
        "feature_2_title": "Sicher",
        "feature_2_desc": "Sicherheit auf Unternehmensniveau",
        "feature_3_title": "Skalierbar",
        "feature_3_desc": "Wächst mit Ihrem Unternehmen"
    },
    "fr": {
        "title": "Bienvenue sur notre site",
        "subtitle": "Construisons l'avenir ensemble",
        "intro": "Nous proposons des solutions innovantes pour les entreprises modernes.",
        "learn_more": "En savoir plus",
        "get_started": "Commencer",
        "features_title": "Nos Fonctionnalités",
        "feature_1_title": "Rapide",
        "feature_1_desc": "Performance ultra-rapide",
        "feature_2_title": "Sécurisé",
        "feature_2_desc": "Sécurité de niveau entreprise",
        "feature_3_title": "Évolutif",
        "feature_3_desc": "Évolue avec votre entreprise"
    }
}
```

---

## Syntax

### Basic Translation Injection

Use square brackets with the path to the JSON file and the key:

```
[path/to/file.json:key_name]
```

The path is relative to the website root.

### Format

```
[relative/path/to/translation.json:key]
```

- **Path**: Relative to website root (e.g., `assets/translations/home.json`)
- **Separator**: Colon (`:`)
- **Key**: The translation key within the language object

### Examples

```html
<!-- Simple text -->
<h1>[assets/translations/home.json:title]</h1>

<!-- In attributes -->
<input placeholder="[assets/translations/navigation.json:search_placeholder]">

<!-- Button text -->
<button>[assets/translations/home.json:get_started]</button>

<!-- Link text -->
<a href="/about">[assets/translations/navigation.json:about]</a>
```

---

## Examples

### Complete HTML Template

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>[assets/translations/home.json:title]</title>
    <link rel="stylesheet" href="/assets/css/style.css">
</head>
<body>
    <!-- Header with navigation -->
    <header>
        <nav>
            <a href="/">[assets/translations/navigation.json:home]</a>
            <a href="/about">[assets/translations/navigation.json:about]</a>
            <a href="/services">[assets/translations/navigation.json:services]</a>
            <a href="/contact">[assets/translations/navigation.json:contact]</a>
        </nav>
        <div class="search">
            <input type="text" placeholder="[assets/translations/navigation.json:search_placeholder]">
        </div>
    </header>
    
    <!-- Hero section -->
    <section class="hero">
        <h1>[assets/translations/home.json:title]</h1>
        <p class="subtitle">[assets/translations/home.json:subtitle]</p>
        <p class="intro">[assets/translations/home.json:intro]</p>
        <div class="cta">
            <a href="/about" class="btn primary">[assets/translations/home.json:learn_more]</a>
            <a href="/register" class="btn secondary">[assets/translations/home.json:get_started]</a>
        </div>
    </section>
    
    <!-- Features section -->
    <section class="features">
        <h2>[assets/translations/home.json:features_title]</h2>
        
        <div class="feature">
            <h3>[assets/translations/home.json:feature_1_title]</h3>
            <p>[assets/translations/home.json:feature_1_desc]</p>
        </div>
        
        <div class="feature">
            <h3>[assets/translations/home.json:feature_2_title]</h3>
            <p>[assets/translations/home.json:feature_2_desc]</p>
        </div>
        
        <div class="feature">
            <h3>[assets/translations/home.json:feature_3_title]</h3>
            <p>[assets/translations/home.json:feature_3_desc]</p>
        </div>
    </section>
    
    <!-- Footer -->
    <footer>
        <p>[assets/translations/footer.json:copyright]</p>
    </footer>
</body>
</html>
```

### Generated Output (English)

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Welcome to Our Website</title>
    <link rel="stylesheet" href="/assets/css/style.css">
</head>
<body>
    <header>
        <nav>
            <a href="/en/">Home</a>
            <a href="/en/about">About Us</a>
            <a href="/en/services">Services</a>
            <a href="/en/contact">Contact</a>
        </nav>
        <div class="search">
            <input type="text" placeholder="Search...">
        </div>
    </header>
    
    <section class="hero">
        <h1>Welcome to Our Website</h1>
        <p class="subtitle">Building the Future Together</p>
        <p class="intro">We provide innovative solutions for modern businesses.</p>
        <div class="cta">
            <a href="/en/about" class="btn primary">Learn More</a>
            <a href="/en/register" class="btn secondary">Get Started</a>
        </div>
    </section>
    
    <!-- ... rest of page ... -->
</body>
</html>
```

### Generated Output (German)

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Willkommen auf unserer Website</title>
    <link rel="stylesheet" href="/assets/css/style.css">
</head>
<body>
    <header>
        <nav>
            <a href="/de/">Startseite</a>
            <a href="/de/about">Über Uns</a>
            <a href="/de/services">Dienstleistungen</a>
            <a href="/de/contact">Kontakt</a>
        </nav>
        <div class="search">
            <input type="text" placeholder="Suchen...">
        </div>
    </header>
    
    <section class="hero">
        <h1>Willkommen auf unserer Website</h1>
        <p class="subtitle">Gemeinsam die Zukunft gestalten</p>
        <p class="intro">Wir bieten innovative Lösungen für moderne Unternehmen.</p>
        <div class="cta">
            <a href="/de/about" class="btn primary">Mehr erfahren</a>
            <a href="/de/register" class="btn secondary">Loslegen</a>
        </div>
    </section>
    
    <!-- ... rest of page ... -->
</body>
</html>
```

---

## Language Routing

### Automatic URL Prefixing

When languages are configured, internal links are automatically prefixed:

| Template Link | Generated (English) | Generated (German) |
|---------------|---------------------|-------------------|
| `href="/"` | `href="/en/"` | `href="/de/"` |
| `href="/about"` | `href="/en/about"` | `href="/de/about"` |
| `href="/contact"` | `href="/en/contact"` | `href="/de/contact"` |

### What's NOT Prefixed

These paths are NOT modified with language prefixes:

- **CSS files**: `href="/assets/css/style.css"` (unchanged)
- **JavaScript files**: `src="/assets/js/main.js"` (unchanged)
- **Images**: `src="/images/logo.png"` (unchanged)
- **Asset paths**: Any path starting with `/assets/`
- **External URLs**: `href="https://example.com"`

### Accessing Languages via URL

```
http://localhost:8080/en/         → English home page
http://localhost:8080/de/         → German home page
http://localhost:8080/fr/about    → French about page
http://localhost:8080/es/contact  → Spanish contact page
```

### Language Switcher Component

Create a component for switching languages:

```html
<!-- components/language-switcher.html -->
<div class="language-switcher">
    <a href="/en/" class="lang-link">EN</a>
    <a href="/de/" class="lang-link">DE</a>
    <a href="/fr/" class="lang-link">FR</a>
</div>
```

---

## Best Practices

### 1. Organize Translation Files by Feature

```
assets/translations/
├── common.json      # Shared across all pages
├── navigation.json  # Navigation and menus
├── home.json        # Home page specific
├── about.json       # About page specific
├── errors.json      # Error messages
├── forms.json       # Form labels and validation
└── footer.json      # Footer content
```

### 2. Use Consistent Key Naming

```json
{
    "en": {
        "page_title": "Page Title",
        "section_hero_title": "Hero Title",
        "section_hero_subtitle": "Hero Subtitle",
        "button_submit": "Submit",
        "button_cancel": "Cancel",
        "error_required": "This field is required",
        "error_invalid_email": "Please enter a valid email"
    }
}
```

### 3. Include All Languages in Each File

Always include all supported languages to avoid missing translations:

```json
{
    "en": { "greeting": "Hello" },
    "de": { "greeting": "Hallo" },
    "fr": { "greeting": "Bonjour" },
    "es": { "greeting": "Hola" }
}
```

### 4. Use Fallback to English

The system automatically falls back to English if a key is missing in the requested language.

### 5. Keep Keys Short but Descriptive

```json
// Good
{
    "nav_home": "Home",
    "hero_title": "Welcome",
    "cta_learn_more": "Learn More"
}

// Avoid
{
    "the_navigation_home_link_text": "Home",
    "h": "Home"
}
```

### 6. Don't Translate Brand Names

Keep brand names, product names, and proper nouns consistent:

```json
{
    "en": {
        "product_name": "Geruest",
        "company_name": "Your Company"
    },
    "de": {
        "product_name": "Geruest",
        "company_name": "Your Company"
    }
}
```

### 7. Handle Pluralization in Keys

```json
{
    "en": {
        "item_count_one": "1 item",
        "item_count_many": "items"
    },
    "de": {
        "item_count_one": "1 Artikel",
        "item_count_many": "Artikel"
    }
}
```

---

## Troubleshooting

### Translation Not Appearing

1. Check the JSON file path is correct (relative to website root)
2. Verify the key exists in the JSON file
3. Ensure the JSON syntax is valid
4. Check that the language code exists in the translation file

### Language Prefix Not Added

1. Verify `setAvailableLanguages()` is called before `init()`
2. Check the path isn't for a static asset (CSS, JS, images)

### Fallback Not Working

1. Make sure "en" language exists in your translation file
2. Verify the key exists in the English section

---

## Next Steps

- [HTML Injections](HTML_INJECTIONS.md) - Component system
- [Features](FEATURES.md) - All features overview
- [Data Classes](DATA_CLASSES.md) - JSONParser reference
