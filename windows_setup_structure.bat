@echo off

REM Create directories
mkdir assets\JSONs
mkdir assets\css
mkdir assets\docs7
mkdir assets\images
mkdir assets\js
mkdir assets\translations
mkdir components
mkdir configs
mkdir files_maps
mkdir html

REM Write to footer.html
(
echo ^<footer^>
echo   ^<p^>^&copy; 2025 Meine-Buecher^</p^>
echo ^</footer^>
) > components\footer.html

REM Write to header.html
(
echo ^<header^>
echo   ^<h1^>Welcome to Meine-Buecher^</h1^>
echo ^</header^>
) > components\header.html

REM Write to restrictions.json
(
echo {
echo   "maxUploadSizeMB": 10,
echo   "allowedFileTypes": ["pdf", "jpg", "png"]
echo }
) > configs\restrictions.json

REM Write to css_file_map.json
(
echo {
echo   "main.css": "/assets/css/main.css"
echo }
) > files_maps\css_file_map.json

REM Write to js_file_map.json
(
echo {
echo   "main.js": "/assets/js/main.js"
echo }
) > files_maps\js_file_map.json

REM Write to index.html
(
echo ^<!DOCTYPE html^>
echo ^<html lang="en"^>
echo ^<head^>
echo   ^<meta charset="UTF-8"^>
echo   ^<title^>Meine-Buecher^</title^>
echo   ^<link rel="stylesheet" href="/assets/css/main.css"^>
echo ^</head^>
echo ^<body^>
echo   ^<!-- Header will be included here -->^
echo   ^<script src="/assets/js/main.js"^>^</script^>
echo ^</body^>
echo ^</html^>
) > html\index.html
