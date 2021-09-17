#include "graphics/core/Texture.hh"
#include "graphics/vulkan/DeviceContext.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <openvr.h>

namespace sp::xr {
    void OpenVrSystem::SubmitView(ecs::XrEye eye, GraphicsContext *context, GpuTexture *tex) {
        Assert(context != nullptr, "TranslateTexture: null GraphicsContext");
        Assert(tex != nullptr, "TranslateTexture: null GpuTexture");
        Assert(tex->GetHandleType() == GpuTexture::HandleType::Vulkan, "TranslateTexture: GpuTexture type != Vulkan");

        vulkan::DeviceContext *device = dynamic_cast<vulkan::DeviceContext *>(context);
        Assert(device != nullptr, "TranslateTexture: GraphicsContext is not a vulkan::DeviceContext");

        vulkan::ImageView *imageView = dynamic_cast<vulkan::ImageView *>(tex);
        Assert(imageView != nullptr, "TranslateTexture: GpuTexture is not a vulkan::ImageView");

        vr::VRVulkanTextureData_t vulkanData;
        vulkanData.m_pDevice = device->Device();
        vulkanData.m_pPhysicalDevice = device->PhysicalDevice();
        vulkanData.m_pInstance = device->Instance();
        vulkanData.m_pQueue = device->GetQueue(vulkan::CommandContextType::General);
        vulkanData.m_nQueueFamilyIndex = device->QueueFamilyIndex(vulkan::CommandContextType::General);

        vulkanData.m_nImage = (uint64_t)(VkImage)(**imageView->Image());
        vulkanData.m_nFormat = (uint32)vk::Format::eR8G8B8A8Srgb;
        vulkanData.m_nWidth = tex->GetWidth();
        vulkanData.m_nHeight = tex->GetHeight();
        vulkanData.m_nSampleCount = 1;

        vr::Texture_t texture = {&vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto};

        // Ignore OpenVR performance warning: https://github.com/ValveSoftware/openvr/issues/818
        device->disabledDebugMessages = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        vr::VRCompositor()->Submit(MapXrEyeToOpenVr(eye), &texture);
        device->disabledDebugMessages = 0;
    }
} // namespace sp::xr
