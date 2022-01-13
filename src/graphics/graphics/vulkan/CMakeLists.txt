add_subdirectory(core)

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
