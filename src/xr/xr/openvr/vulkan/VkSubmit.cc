#if defined(SP_XR_SUPPORT_OPENVR) && defined(SP_GRAPHICS_SUPPORT_VK)

    #include "graphics/core/Texture.hh"
    #include "graphics/vulkan/core/DeviceContext.hh"
    #include "xr/openvr/OpenVrSystem.hh"

    #include <openvr.h>

namespace sp::xr {
    void OpenVrSystem::SubmitView(ecs::XrEye eye, glm::mat4 &viewPose, GpuTexture *tex) {
        if (!loaded.test()) return;

        Assert(context, "TranslateTexture: null GraphicsContext");
        Assert(tex, "TranslateTexture: null GpuTexture");

        vulkan::DeviceContext *device = dynamic_cast<vulkan::DeviceContext *>(context);
        Assert(device, "TranslateTexture: GraphicsContext is not a vulkan::DeviceContext");

        vulkan::ImageView *imageView = dynamic_cast<vulkan::ImageView *>(tex);
        Assert(imageView, "TranslateTexture: GpuTexture is not a vulkan::ImageView");

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

        vr::VRTextureWithPose_t texture;
        texture.handle = &vulkanData;
        texture.eType = vr::TextureType_Vulkan;
        texture.eColorSpace = vr::ColorSpace_Auto;
        glm::mat3x4 trackingPose = glm::transpose(viewPose);
        memcpy((float *)texture.mDeviceToAbsoluteTracking.m,
            glm::value_ptr(trackingPose),
            sizeof(texture.mDeviceToAbsoluteTracking.m));

        auto vrEye = MapXrEyeToOpenVr(eye);

        // Work around OpenVR barrier bug: https://github.com/ValveSoftware/openvr/issues/1591
        if (vrEye == vr::Eye_Right) {
            if (texWidth != tex->GetWidth() || texHeight != tex->GetHeight()) {
                texWidth = tex->GetWidth();
                texHeight = tex->GetHeight();
                frameCountWorkaround = 0;
            }
            if (frameCountWorkaround < 4) {
                vulkanData.m_unArrayIndex = 0;
                frameCountWorkaround++;
            }
        }

        // Ignore OpenVR performance warning: https://github.com/ValveSoftware/openvr/issues/818
        device->disabledDebugMessages = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        auto err = vr::VRCompositor()->Submit(vrEye,
            &texture,
            0,
            (vr::EVRSubmitFlags)(vr::Submit_TextureWithPose | vr::Submit_VulkanTextureWithArrayData));
        device->disabledDebugMessages = 0;
        Assert(err == vr::VRCompositorError_None || err == vr::VRCompositorError_DoNotHaveFocus,
            "VR compositor error: " + std::to_string(err));
    }
} // namespace sp::xr

#endif
