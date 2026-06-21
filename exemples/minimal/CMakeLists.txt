add_executable(minimal minimal.cpp)
target_include_directories(minimal PRIVATE ${GERUEST_SRC})
target_link_libraries(minimal PRIVATE Geruest)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(minimal PRIVATE "-s")
endif()

add_custom_command(TARGET minimal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/website
    $<TARGET_FILE_DIR:minimal>/website
    COMMENT "Copying minimal website to build directory"
)
