find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

add_library(${PROJECT_GRAPHICS_CORE_LIB} STATIC)
# add_library(${PROJECT_GRAPHICS_EGL_LIB} STATIC)
# add_library(${PROJECT_GRAPHICS_OPENGL_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_XR_LIB} STATIC)

target_include_directories(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PRIVATE
    "$<TARGET_PROPERTY:glfw,INCLUDE_DIRECTORIES>"
)

target_link_libraries(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ImGui
)

# target_link_libraries(${PROJECT_GRAPHICS_EGL_LIB} PUBLIC
#     ${PROJECT_GRAPHICS_CORE_LIB}
#     GLEW
#     glfw-egl
#     OpenGL::GL
# )
# target_link_libraries(${PROJECT_GRAPHICS_OPENGL_LIB} PUBLIC
#     ${PROJECT_GRAPHICS_CORE_LIB}
#     GLEW
#     glfw
#     OpenGL::GL
# )
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC
    ${PROJECT_GRAPHICS_CORE_LIB}
    Vulkan::Vulkan
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PUBLIC
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    glfw-egl # Prevents X11 context creation
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_XR_LIB} PUBLIC
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    ${PROJECT_XR_CORE_LIB}
    glfw
)

target_compile_definitions(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT
)
# target_compile_definitions(${PROJECT_GRAPHICS_EGL_LIB} PUBLIC
#     SP_GRAPHICS_SUPPORT_GL
# )
# target_compile_definitions(${PROJECT_GRAPHICS_OPENGL_LIB} PUBLIC
#     SP_GRAPHICS_SUPPORT_GL
# )
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_VK
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_HEADLESS
)

target_precompile_headers(${PROJECT_GRAPHICS_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
# target_precompile_headers(${PROJECT_GRAPHICS_EGL_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
# target_precompile_headers(${PROJECT_GRAPHICS_OPENGL_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_XR_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(graphics)

add_module_sources(
    GraphicsManager.cc
)
