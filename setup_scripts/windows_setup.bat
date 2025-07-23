@echo off

REM Create website directory and subdirectories
mkdir website\assets\JSONs
mkdir website\assets\css
mkdir website\assets\docs
mkdir website\assets\images
mkdir website\assets\js
mkdir website\assets\translations
mkdir website\components
mkdir website\configs
mkdir website\files_maps
mkdir website\html

REM Write to footer.html
(
echo ^<footer^>
echo   ^<p^>^&copy; 2025 Meine-Buecher^</p^>
echo ^</footer^>
) > website\components\footer.html

REM Write to header.html
(
echo ^<header^>
echo   ^<h1^>Welcome to Meine-Buecher^</h1^>
echo ^</header^>
) > website\components\header.html

REM Write to restrictions.json
(
echo {
echo   "maxUploadSizeMB": 10,
echo   "allowedFileTypes": ["pdf", "jpg", "png"]
echo }
) > website\configs\restrictions.json

REM Write to css_file_map.json
(
echo {
echo   "main.css": "/assets/css/main.css"
echo }
) > website\files_maps\css_file_map.json

REM Write to js_file_map.json
(
echo {
echo   "main.js": "/assets/js/main.js"
echo }
) > website\files_maps\js_file_map.json

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
) > website\html\index.html
