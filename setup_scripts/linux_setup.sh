#!/bin/bash

# Create directories
mkdir -p assets/{JSONs,css,docs7,images,js,translations}
mkdir -p components
mkdir -p configs
mkdir -p files_maps
mkdir -p html

# Create files with default content
cat > components/footer.html <<EOF
<footer>
  <p>&copy; 2025 Meine-Buecher</p>
</footer>
EOF

cat > components/header.html <<EOF
<header>
  <h1>Welcome to Meine-Buecher</h1>
</header>
EOF

cat > configs/restrictions.json <<EOF
{
  "maxUploadSizeMB": 10,
  "allowedFileTypes": ["pdf", "jpg", "png"]
}
EOF

cat > files_maps/css_file_map.json <<EOF
{
  "main.css": "/assets/css/main.css"
}
EOF

cat > files_maps/js_file_map.json <<EOF
{
  "main.js": "/assets/js/main.js"
}
EOF

cat > html/index.html <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Meine-Buecher</title>
  <link rel="stylesheet" href="/assets/css/main.css">
</head>
<body>
  <!-- Header will be included here -->
  <script src="/assets/js/main.js"></script>
</body>
</html>
EOF
