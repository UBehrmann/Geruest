# HTML Component Injection

Include reusable components in templates using `{component_path}` syntax.

## Syntax

```html
{components/header.html}  <!-- No spaces in braces -->
{components/cards/product-card.html}  <!-- Supports subdirectories -->
```

- Path relative to website root
- Include file extension
- Nested injections supported (components can include other components)

## Examples

**Template** (`html/index.html`):
```html
<!DOCTYPE html>
<html>
<head><title>My Site</title></head>
<body>
    {components/header.html}
    <main><h1>Welcome</h1></main>
    {components/footer.html}
</body>
</html>
```

**Component** (`components/header.html`):
```html
<header>
    {components/navigation.html}
    <div class="actions">
        <button>Menu</button>
    </div>
</header>
```

**Result**: All components are inline-replaced at build time.

## With Translations

Components work with translations:
```html
<!-- components/header.html -->
<header>
    <nav>
        <a href="/">[assets/translations/navigation.json:home]</a>
        <a href="/about">[assets/translations/navigation.json:about]</a>
    </nav>
</header>
```

Both component injection and translations are processed!