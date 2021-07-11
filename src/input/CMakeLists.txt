add_library(${PROJECT_INPUT_CORE_LIB} STATIC)
add_library(${PROJECT_INPUT_GLFW_LIB} STATIC)

target_include_directories(${PROJECT_INPUT_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_INPUT_CORE_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    ${PROJECT_INPUT_CORE_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    glfw
)

target_compile_definitions(${PROJECT_INPUT_CORE_LIB} PUBLIC
    SP_INPUT_SUPPORT
)
target_compile_definitions(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    SP_INPUT_SUPPORT_GLFW
)

add_subdirectory(input)
