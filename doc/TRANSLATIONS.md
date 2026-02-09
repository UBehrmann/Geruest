# Translations

Multi-language support with JSON-based translation files and automatic HTML processing.

## Setup

```cpp
// First language is default
server.setAvailableLanguages({"en", "de", "fr"});
```

## Syntax

`[path/to/file.json:key]` - Path relative to website root

```html
<h1>[assets/translations/home.json:title]</h1>
<input placeholder="[assets/translations/navigation.json:search]">
<a href="/about">[assets/translations/navigation.json:about]</a>
```

## Translation Files

**`assets/translations/navigation.json`:**
```json
{
    "en": { "home": "Home", "about": "About", "search": "Search..." },
    "de": { "home": "Startseite", "about": "Über Uns", "search": "Suchen..." },
    "fr": { "home": "Accueil", "about": "À Propos", "search": "Rechercher..." }
}
```

## How It Works

1. URLs automatically prefixed with language codes: `/about` → `/en/about`
2. HTML templates processed with language-specific translations
3. Generated pages saved to `html/en/`, `html/de/`, etc.
4. Static assets (CSS/JS/images) remain unprefixed

## Directory Structure

```
website/
├── assets/translations/
│   ├── navigation.json
│   ├── home.json
│   └── errors.json
└── html/
    ├── index.html     # Template
    ├── en/
    │   └── index.html # Generated
    ├── de/
    │   └── index.html # Generated
    └── fr/
        └── index.html # Generated
```

## With Components

Components work with translations:
```html
<!-- components/header.html -->
<nav>
    <a href="/">[assets/translations/navigation.json:home]</a>
    <a href="/about">[assets/translations/navigation.json:about]</a>
</nav>

<!-- html/index.html -->
{components/header.html}  <!-- Component with translations is processed -->
<h1>[assets/translations/home.json:welcome]</h1>
```

Both component injection AND translations are processed correctly!
