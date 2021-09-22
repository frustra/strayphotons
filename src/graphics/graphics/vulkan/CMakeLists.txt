target_sources(${PROJECT_GRAPHICS_VULKAN_LIB} PRIVATE
    CommandContext.cc
    Common.cc
    DeviceContext.cc
    GuiRenderer.cc
    Image.cc
    Memory.cc
    Model.cc
    Pipeline.cc
    Renderer.cc
    RenderGraph.cc
    RenderPass.cc
    RenderTarget.cc
    Screenshot.cc
    UniqueID.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_LIB} PUBLIC VulkanMemoryAllocator SPIRV-Reflect)
