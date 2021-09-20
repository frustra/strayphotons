#include "graphics/core/Texture.hh"
#include "graphics/vulkan/DeviceContext.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <openvr.h>

namespace sp::xr {
    void OpenVrSystem::SubmitView(ecs::XrEye eye, GpuTexture *tex) {
        Assert(context != nullptr, "TranslateTexture: null GraphicsContext");
        Assert(tex != nullptr, "TranslateTexture: null GpuTexture");

        vulkan::DeviceContext *device = dynamic_cast<vulkan::DeviceContext *>(context);
        Assert(device != nullptr, "TranslateTexture: GraphicsContext is not a vulkan::DeviceContext");

        vulkan::ImageView *imageView = dynamic_cast<vulkan::ImageView *>(tex);
        Assert(imageView != nullptr, "TranslateTexture: GpuTexture is not a vulkan::ImageView");

        vr::VRVulkanTextureArrayData_t vulkanData;
        vulkanData.m_pDevice = device->Device();
        vulkanData.m_pPhysicalDevice = device->PhysicalDevice();
        vulkanData.m_pInstance = device->Instance();
        vulkanData.m_pQueue = device->GetQueue(vulkan::CommandContextType::General);
        vulkanData.m_nQueueFamilyIndex = device->QueueFamilyIndex(vulkan::CommandContextType::General);

        auto image = imageView->Image();
        Assert(image->ArrayLayers() == 2, "image must have 2 array layers");

        vulkanData.m_nImage = (uint64_t)(VkImage)(**image);
        vulkanData.m_unArraySize = image->ArrayLayers();
        vulkanData.m_unArrayIndex = (uint32)eye;
        vulkanData.m_nFormat = (uint32)image->Format();
        vulkanData.m_nWidth = tex->GetWidth();
        vulkanData.m_nHeight = tex->GetHeight();
        vulkanData.m_nSampleCount = 1;

        vr::Texture_t texture = {&vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto};
        auto vrEye = MapXrEyeToOpenVr(eye);

        // Work around OpenVR barrier bug: https://github.com/ValveSoftware/openvr/issues/1591
        if (vrEye == vr::Eye_Right) {
            if (rightEyeTexture != tex) {
                rightEyeTexture = tex;
                frameCountWorkaround = 0;
            }
            if (frameCountWorkaround < 4) {
                vulkanData.m_unArrayIndex = 0;
                frameCountWorkaround++;
            }
        }

        // Ignore OpenVR performance warning: https://github.com/ValveSoftware/openvr/issues/818
        device->disabledDebugMessages = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        auto err = vr::VRCompositor()->Submit(vrEye, &texture, 0, vr::Submit_VulkanTextureWithArrayData);
        device->disabledDebugMessages = 0;
        Assert(err == vr::VRCompositorError_None || err == vr::VRCompositorError_DoNotHaveFocus,
               "VR compositor error: " + std::to_string(err));
    }
} // namespace sp::xr
