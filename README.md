# Geruest

![Geruest Framework](imgs/geruest_image_ai.png)
*Image generated with OpenAI ChatGPT. Looking to commission a better logo/image - if you're a designer interested in contributing, please reach out!*

## Description

Geruest (German for "scaffold") is a lightweight C++ web framework designed to simplify the creation of web applications. It provides a straightforward API for routing, serving static files, and handling requests, making it easy to build and deploy web services.

## Features

- Configurable
  - Port
  - Hostname
- No SSL support (perhaps in the future but needs to be with Let's Encrypt for free SSL)
  - Use 'traefik' for SSL termination
- Standard content folder system for html, css, js, images, etc. files
- Multilanguage support
- Static file serving
- Routing
  - simple add route function `addRoute(path, callback);`
- **Asset Merging** (NEW)
  - Automatically scan HTML templates for CSS/JS includes
  - Merge multiple files into single bundled files per page
  - Reduces HTTP requests and eliminates manual JSON mapping
- Security
  - Logic bomb for bots?
  - Rate limiting
  - IP blocking
- CLI for easy management of the server while running

## Roadmap

- [X] basic configuration
- [X] Static file serving
- [X] Multilanguage support 
- [X] Simple routing
- [X] Test the first iteration with existing code base
- [ ] Add CLI
- [ ] Add Rate limiting
- [ ] Add IP blocking
- [ ] Add websockets support

## Requirements

- C++17 or later
- CMake 3.10+
- Threads
- A compatible C++ compiler (e.g., GCC, Clang, MSVC)

## Quick Start

**One-command installation:**

### Windows (PowerShell)
```powershell
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release
```

### Linux/macOS (Bash)
```bash
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && sudo cmake --install .
```

## Documentation

**→ [Complete Documentation Index](doc/README.md)**

### Getting Started

- **[Getting Started Guide](doc/GETTING_STARTED.md)** - Installation, requirements, and your first server
- **[Usage Guide](doc/USAGE_GUIDE.md)** - Local development, Docker deployment, production setup
- **[Configuration Guide](doc/CONFIGURATION.md)** - .env files, environment variables, configuration hierarchy

### Core Features

- **[Features Overview](doc/FEATURES.md)** - All features with examples
  - Routing (exact and wildcard patterns)
  - Static file serving
  - Thread pool configuration
  - Multi-language support
  - Graceful shutdown
- **[Data Classes Reference](doc/DATA_CLASSES.md)** - HTTPRequest, HTTPResponse, JSONParser API

### Template System

- **[HTML Injections](doc/HTML_INJECTIONS.md)** - Reusable component system
- **[Translations](doc/TRANSLATIONS.md)** - Multi-language translation injection
- **[Asset Merging](doc/ASSET_MERGING.md)** - Automatic CSS/JS bundling per page

### Security

- **[Basic Authentication](doc/BASIC_AUTH.md)** - HTTP Basic Auth for protected pages

### Contributing

- **[Contributing Guide](doc/CONTRIBUTING.md)** - Development setup, code style, testing guidelines



## Folder Structure

```
/assets
├── JSONs
├── css/
├── docs/
├── images/
├── js/
└──  translations/
/components
├── footer.html
└── header.html
/configs
└── restrictions.json
/files_maps
├── css_file_map.json
└── js_file_map.json
/html
└── index.html
```

## How to use the framework

**See the [Getting Started Guide](doc/GETTING_STARTED.md) for detailed instructions.**

### Quick Example

```cpp
#include <Geruest.hpp>

int main() {
    geruest::Geruest server;

    // Configure server settings
    server.setPort(8080);
    server.setHostname("localhost");
    server.addRoot("/path/to/your/website");

    // Define a simple route
    server.addRoute("/hello", [](const geruest::HTTPRequest& req) {
        geruest::HTTPResponse response("200 OK");
        response.setHeader("Content-Type", "text/html");
        response.setBody("<h1>Welcome to Geruest!</h1>");
        return response;
    });

    // Start the server
    server.init();
    server.start();

    return 0;
}
```

**For more examples, see:**
- [Getting Started Guide](doc/GETTING_STARTED.md)
- [Features Documentation](doc/FEATURES.md)
- [Example Application](exemple/exemple.cpp)

## Installation

**See the [Getting Started Guide](doc/GETTING_STARTED.md) for comprehensive installation instructions.**

### Quick Install

### Quick Install

Clone the Geruest repository:

```bash
git clone https://github.com/UBehrmann/Geruest.git
cd Geruest
```

**Build instructions for your platform:**
- [Linux Installation](doc/GETTING_STARTED.md#linux)
- [Windows (MSVC) Installation](doc/GETTING_STARTED.md#windows-msvc)
- [Windows (MinGW) Installation](doc/GETTING_STARTED.md#windows-mingw)

We build the library for both Linux and Windows:
- `.a` file for Linux
- `.a` file for Windows using MinGW  
- `.lib` file for Windows using MSVC 

### Using Geruest in Your Project

**See [Getting Started - CMakeLists.txt](doc/GETTING_STARTED.md#cmakeliststxt-for-your-project) for a complete example.**

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyWebsiteApp)

add_executable(MyWebsiteApp main.cpp)

# Link to your library
target_link_libraries(MyWebsiteApp
    PRIVATE
    Geruest
)

# Include the library headers
target_include_directories(MyWebsiteApp
    PRIVATE
    /path/to/Geruest/src
)

# Add the library binary directory (where the .lib/.a is)
link_directories(/path/to/Geruest/build)
```

## CLI Commands

To connect to the server's CLI:

```bash
nc ip_address port
```

Available commands:

| Command   | Description                            |
| --------- | -------------------------------------- |
| `version` | Show the current version of the server |
| `status`  | Show the current status of the server  |
| `help`    | Show available commands                |
| `exit`    | Exit the CLI                           |
| `clear`   | Clear the CLI screen                   |
| `config`  | Show current server configuration      |
| `uptime`  | Show server uptime                     |
| `routes`  | Show all defined routes                |

## License

This project is licensed under the MIT License.
