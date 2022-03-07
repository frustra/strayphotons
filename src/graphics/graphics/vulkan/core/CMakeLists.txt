target_sources(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PRIVATE
    BufferPool.cc
    CommandContext.cc
    Common.cc
    DeviceContext.cc
    Image.cc
    Memory.cc
    PerfTimer.cc
    Pipeline.cc
    RenderPass.cc
    RenderTarget.cc
    UniqueID.cc
    Util.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC VulkanMemoryAllocator SPIRV-Reflect)
