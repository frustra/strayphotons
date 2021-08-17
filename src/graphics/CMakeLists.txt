find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

add_library(${PROJECT_GRAPHICS_CORE_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_EGL_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_OPENGL_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_LIB} STATIC)

target_include_directories(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
)

target_link_libraries(${PROJECT_GRAPHICS_EGL_LIB} PUBLIC
    ${PROJECT_GRAPHICS_CORE_LIB}
    GLEW
    glfw-egl
    ImGui
    OpenGL::GL
)
target_link_libraries(${PROJECT_GRAPHICS_OPENGL_LIB} PUBLIC
    ${PROJECT_GRAPHICS_CORE_LIB}
    GLEW
    glfw
    ImGui
    OpenGL::GL
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_LIB} PUBLIC
    ${PROJECT_GRAPHICS_CORE_LIB}
    ${PROJECT_INPUT_CORE_LIB}
    glfw-egl
    ImGui
    vulkan
)

target_compile_definitions(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT
)
target_compile_definitions(${PROJECT_GRAPHICS_EGL_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_GL
)
target_compile_definitions(${PROJECT_GRAPHICS_OPENGL_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_GL
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_VK
)

add_subdirectory(graphics)

add_module_sources(
    GraphicsManager.cc
)
