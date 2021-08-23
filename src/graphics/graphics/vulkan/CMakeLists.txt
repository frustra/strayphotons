target_sources(${PROJECT_GRAPHICS_VULKAN_LIB} PRIVATE
    Common.cc
    VulkanGraphicsContext.cc
    VulkanMemory.cc
    VulkanRenderer.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_LIB} PUBLIC VulkanMemoryAllocator)
