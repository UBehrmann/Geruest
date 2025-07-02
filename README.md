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

- [ ]

## Requirements

- C++17 or later
- CMake 3.10+
- POSIX-compatible system (Linux, macOS)

## Build Instructions

```bash
git clone https://github.com/UBehrmann/geruest.git
cd geruest
mkdir build && cd build
cmake ..
make
```

## Folder Structure

```
/assets
├── JSONs
├── css/
├── docs7
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
#include "geruest.h"

int main() {
    Geruest server;

    // Configure server settings
    server.setPort(8080);
    server.setHostname("localhost");

    // Define a simple route
    server.addRoute("/", [](const Request& req, Response& res) {
        res.setContentType("text/html");
        res.send("<h1>Welcome to Geruest!</h1>");
    });

    // Start the server
    server.start();

    return 0;
}
```

## Installation

Download the Geruest library from the official repository and include it in your C++ project. Ensure you have a compatible C++ compiler and standard library.

### CMakeLists example

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyWebsiteApp)

add_executable(MyWebsiteApp main.cpp)

# Link to your library
target_link_libraries(MyWebsiteApp
    PRIVATE
    CppWebFramework
)

# Include the library headers
target_include_directories(MyWebsiteApp
    PRIVATE
    /path/to/CppWebFramework/include
)

# Add the library binary directory (where the .lib/.a is)
link_directories(/path/to/CppWebFramework/build)
```

## CLI Commands

### Connect to the server

```bash
nc ip_address port
```

### CLI Commands

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
