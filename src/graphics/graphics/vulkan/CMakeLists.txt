add_subdirectory(core)
add_subdirectory(render_graph)
add_subdirectory(render_passes)

target_sources(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PRIVATE
    GPUSceneContext.cc
    GuiRenderer.cc
    Renderer.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_XR_LIB} PRIVATE
    GPUSceneContext.cc
    GuiRenderer.cc
    Renderer.cc
)
