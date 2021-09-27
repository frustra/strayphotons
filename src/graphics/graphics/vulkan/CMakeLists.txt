add_subdirectory(core)

target_sources(${PROJECT_GRAPHICS_VULKAN_FLAT_LIB} PRIVATE
    Renderer.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_XR_LIB} PRIVATE
    Renderer.cc
)
