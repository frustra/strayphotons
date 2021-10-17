add_library(${PROJECT_INPUT_GLFW_LIB} STATIC)

target_include_directories(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    glfw
)

target_compile_definitions(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    SP_INPUT_SUPPORT_GLFW
)

target_precompile_headers(${PROJECT_INPUT_GLFW_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(input)
