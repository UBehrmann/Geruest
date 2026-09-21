# Boost.System is header-only on recent packages (e.g. Ubuntu 26.04 / Boost 1.90),
# which ship no boost_system CMake config. Prefer COMPONENTS system when present;
# otherwise Boost::headers is enough, and we alias Boost::system for link lines.

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.75 QUIET COMPONENTS system)
if(NOT Boost_FOUND)
    find_package(Boost 1.75 REQUIRED)
endif()

if(NOT TARGET Boost::system)
    if(NOT TARGET Boost::headers)
        message(FATAL_ERROR "Boost found but neither Boost::system nor Boost::headers exists")
    endif()
    add_library(Boost::system INTERFACE IMPORTED GLOBAL)
    target_link_libraries(Boost::system INTERFACE Boost::headers)
endif()
