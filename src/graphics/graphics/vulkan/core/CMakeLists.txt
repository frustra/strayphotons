target_sources(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PRIVATE
    CommandContext.cc
    Common.cc
    DeviceContext.cc
    GuiRenderer.cc
    Image.cc
    Memory.cc
    Model.cc
    Pipeline.cc
    RenderGraph.cc
    RenderPass.cc
    RenderTarget.cc
    Screenshot.cc
    UniqueID.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC VulkanMemoryAllocator SPIRV-Reflect)
