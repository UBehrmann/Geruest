# Geruest

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

## 🚀 Quick Start

**One-command installation:**

### Windows (PowerShell)
```powershell
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release
```

### Linux/macOS (Bash)
```bash
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && sudo cmake --install .
```

## 📚 Documentation

- **[Quick Start Guide](./doc/QUICK_START.md)** - Get up and running in minutes
- **[Build & Install Scripts](./doc/BUILD_SCRIPTS.md)** - Copy-paste build commands
- **[Detailed Build Instructions](./doc/BUILD_INSTRUCTIONS.md)** - Complete build information
- **[Installation Guide](./doc/INSTALLATION_INSTRUCTIONS.md)** - Advanced installation options

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

1. Include the Geruest header file in your C++ project.
2. Create an instance of the Geruest class.
3. Configure the server settings such as port and hostname.
4. Define routes using the `addRoute` method, specifying the path and the callback function to handle requests.
5. Start the server to listen for incoming requests.

### Example

```cpp
#include "Geruest.hpp"

int main() {
    Geruest server;

    // Configure server settings
    server.setPort(8080);
    server.setHostname("localhost");
    server.addRoot("/path/to/your/website"); // Set the root directory for static files

    // Define a simple route
    server.addRoute("/hello", [](const Request& req, Response& res) {
        res.setContentType("text/html");
        res.send("<h1>Welcome to Geruest!</h1>");
    });

    // Start the server
    server.start();

    return 0;
}
```

## Installation

Clone the Geruest repository and include it in your C++ project:

```bash
git clone https://github.com/UBehrmann/Geruest.git
```

Ensure you have a compatible C++ compiler and standard library.

We build the library for both Linux and Windows:
- `.a` file for Linux
- `.a` file for Windows using MinGW  
- `.lib` file for Windows using MSVC 

### CMakeLists example

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
