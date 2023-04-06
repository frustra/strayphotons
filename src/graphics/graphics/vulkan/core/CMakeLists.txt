target_sources(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PRIVATE
    Access.cc
    BufferPool.cc
    CommandContext.cc
    DeviceContext.cc
    Image.cc
    Memory.cc
    PerfTimer.cc
    Pipeline.cc
    RenderPass.cc
    UniqueID.cc
    Util.cc
    VkCommon.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC VulkanMemoryAllocator SPIRV-Reflect)
