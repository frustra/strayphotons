add_library(${PROJECT_INPUT_CORE_LIB} STATIC)
add_library(${PROJECT_INPUT_GLFW_LIB} STATIC)

target_include_directories(${PROJECT_INPUT_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_INPUT_CORE_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_INPUT_GLFW_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    glfw
)

add_subdirectory(input)
