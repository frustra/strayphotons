target_sources(${PROJECT_GRAPHICS_VULKAN_LIB} PRIVATE
    Common.cc
    GraphicsContext.cc
    Memory.cc
    Model.cc
    Renderer.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_LIB} PUBLIC VulkanMemoryAllocator)
