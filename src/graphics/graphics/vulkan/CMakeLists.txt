add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(render_graph)
add_subdirectory(render_passes)
add_subdirectory(scene)

target_sources(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PRIVATE
    Renderer.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_XR_LIB} PRIVATE
    Renderer.cc
)
