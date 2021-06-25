set(PROJECT_INPUT_CORE_LIB ${PROJECT_BASE_NAME}-input-core CACHE INTERNAL "Library containing generic user input support")
set(PROJECT_INPUT_GLFW_LIB ${PROJECT_BASE_NAME}-input-glfw CACHE INTERNAL "Library implementing user input support using GLFW")

add_library(${PROJECT_INPUT_CORE_LIB} STATIC)
add_library(${PROJECT_INPUT_GLFW_LIB} STATIC)

target_include_directories(${PROJECT_INPUT_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_INPUT_CORE_LIB} PRIVATE
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_INPUT_GLFW_LIB}
    PRIVATE
        ${PROJECT_CORE_LIB}
        ${PROJECT_INPUT_CORE_LIB}
        ${PROJECT_GRAPHICS_CORE_LIB}
    PUBLIC
        glfw
)

add_subdirectory(input)
