add_executable(showcase showcase.cpp)
target_include_directories(showcase PRIVATE ${GERUEST_SRC})
target_link_libraries(showcase PRIVATE Geruest)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(showcase PRIVATE "-s")
endif()

add_custom_command(TARGET showcase POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/website
    $<TARGET_FILE_DIR:showcase>/website
    COMMENT "Copying showcase website to build directory"
)
