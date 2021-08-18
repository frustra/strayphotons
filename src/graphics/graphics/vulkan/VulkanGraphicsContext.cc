#include "VulkanGraphicsContext.hh"

#include "core/Logging.hh"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sp {
    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                        void *pGraphicsContext) {
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            Errorf("Vulkan Error %s: %s",
                   vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)),
                   pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            Logf("Vulkan Warning %s: %s",
                 vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)),
                 pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            Logf("Vulkan Info %s: %s",
                 vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)),
                 pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            Debugf("Vulkan Verbose %s: %s",
                   vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)),
                   pCallbackData->pMessage);
            break;
        }
        return VK_FALSE;
    }

    static void glfwErrorCallback(int error, const char *message) {
        Errorf("GLFW returned %d: %s", error, message);
    }

    VulkanGraphicsContext::VulkanGraphicsContext() {
        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit()) { throw "glfw failed"; }

        Assert(glfwVulkanSupported(), "Vulkan not supported");

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

        // Disable OpenGL context creation
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        Logf("Available Vulkan extensions: %u", availableExtensions.size());
        for (auto &ext : availableExtensions) {
            Logf("\t%s", ext.extensionName);
        }

        auto availableLayers = vk::enumerateInstanceLayerProperties();
        Logf("Available Vulkan layers: %u", availableLayers.size());
        for (auto &layer : availableLayers) {
            Logf("\t%s %s", layer.layerName, layer.description);
        }

        std::vector<const char *> extensions, layers;
        uint32_t requiredExtensionCount = 0;
        auto requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
        for (uint32_t i = 0; i < requiredExtensionCount; i++) {
            extensions.emplace_back(requiredExtensions[i]);
            Logf("Required extension: %s", requiredExtensions[i]);
        }
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#if SP_DEBUG
        Logf("Running with vulkan validation layers");
        layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

        std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        // Create window and surface
        auto initialSize = CVarWindowSize.Get();
        window = glfwCreateWindow(initialSize.x, initialSize.y, "STRAY PHOTONS", nullptr, nullptr);
        Assert(window, "glfw window creation failed");

        vk::ApplicationInfo applicationInfo("Stray Photons",
                                            VK_MAKE_VERSION(1, 0, 0),
                                            "Stray Photons",
                                            VK_MAKE_VERSION(1, 0, 0),
                                            VK_API_VERSION_1_2);
        vk::InstanceCreateInfo createInfo(vk::InstanceCreateFlags(),
                                          &applicationInfo,
                                          layers.size(),
                                          layers.data(),
                                          extensions.size(),
                                          extensions.data());
#if SP_DEBUG
        vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
        debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
        debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
        debugInfo.pfnUserCallback = &VulkanDebugCallback;
        debugInfo.pUserData = this;
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo;
#endif
        instance = vk::createInstanceUnique(createInfo);
        dispatcher = vk::DispatchLoaderDynamic(*instance, vkGetInstanceProcAddr);
#if SP_DEBUG
        debugMessenger = instance->createDebugUtilsMessengerEXTUnique(debugInfo, nullptr, dispatcher);
#endif

        vk::SurfaceKHR glfwSurface;
        auto result = glfwCreateWindowSurface(*instance, window, nullptr, (VkSurfaceKHR *)&glfwSurface);
        Assert(result == VK_SUCCESS, "Failed to create window surface");
        surface.reset(std::move(glfwSurface));

        auto physicalDevices = instance->enumeratePhysicalDevices(dispatcher);
        // TODO: Prioritize discrete GPUs and check for capabilities like Geometry/Compute shaders
        vk::PhysicalDevice physicalDevice;
        for (auto &dev : physicalDevices) {
            // TODO: Check device extension support
            auto properties = dev.getProperties(dispatcher);
            // auto features = device.getFeatures(dispatcher);
            Logf("Using graphics device: %s", properties.deviceName);
            physicalDevice = dev;
            break;
        }
        Assert(physicalDevice, "No suitable graphics device found!");

        auto queueFamilies = physicalDevice.getQueueFamilyProperties(dispatcher);
        std::optional<uint32_t> graphicsQueueFamily;
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsQueueFamily = i;
                break;
            }
        }
        Assert(graphicsQueueFamily.has_value(), "Couldn't find a Graphics queue family");

        std::optional<uint32_t> presentQueueFamily;
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (physicalDevice.getSurfaceSupportKHR(i, *surface, dispatcher)) {
                presentQueueFamily = i;
                break;
            }
        }
        Assert(presentQueueFamily.has_value(), "Couldn't find a Present queue family");

        std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily.value(), presentQueueFamily.value()};
        std::vector<vk::DeviceQueueCreateInfo> queueInfos;
        float queuePriority = 1.0f;
        for (auto queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueInfo;
            queueInfo.queueFamilyIndex = graphicsQueueFamily.value();
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueInfos.push_back(queueInfo);
        }

        vk::PhysicalDeviceFeatures deviceFeatures;

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.queueCreateInfoCount = queueInfos.size();
        deviceInfo.pQueueCreateInfos = queueInfos.data();
        deviceInfo.pEnabledFeatures = &deviceFeatures;
        deviceInfo.enabledExtensionCount = deviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        deviceInfo.enabledLayerCount = layers.size();
        deviceInfo.ppEnabledLayerNames = layers.data();
        device = physicalDevice.createDeviceUnique(deviceInfo, nullptr, dispatcher);
        graphicsQueue = device->getQueue(graphicsQueueFamily.value(), 0, dispatcher);
        presentQueue = device->getQueue(presentQueueFamily.value(), 0, dispatcher);

        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface, dispatcher);
        auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*surface, dispatcher);
        auto presentModes = physicalDevice.getSurfacePresentModesKHR(*surface, dispatcher);
        // TODO: Actually validate surface capabilities/formats/present modes are available

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        for (auto &mode : presentModes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                presentMode = vk::PresentModeKHR::eMailbox;
                break;
            }
        }
        vk::SurfaceFormatKHR surfaceFormat = surfaceFormats[0];
        for (auto &format : surfaceFormats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                surfaceFormat = format;
                break;
            }
        }
        vk::SwapchainCreateInfoKHR swapChainInfo;
        swapChainInfo.surface = *surface;
        swapChainInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
        swapChainInfo.imageFormat = surfaceFormat.format;
        swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
        // TODO: Check capabilities.currentExtent is valid and correctly handles high dpi
        swapChainInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapChainInfo.imageArrayLayers = 1;
        // TODO: use vk::ImageUsageFlagBits::eTransferDst for rendering from another texture
        swapChainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        if (graphicsQueueFamily != presentQueueFamily) {
            uint32_t queueFamilyIndices[] = {graphicsQueueFamily.value(), presentQueueFamily.value()};
            swapChainInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapChainInfo.queueFamilyIndexCount = 2;
            swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            swapChainInfo.imageSharingMode = vk::SharingMode::eExclusive;
        }
        swapChainInfo.preTransform = surfaceCapabilities.currentTransform;
        swapChainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapChainInfo.presentMode = presentMode;
        swapChainInfo.clipped = true;
        swapChainInfo.oldSwapchain = nullptr;
        swapChain = device->createSwapchainKHRUnique(swapChainInfo, nullptr, dispatcher);
    }

    VulkanGraphicsContext::~VulkanGraphicsContext() {
        if (window) { glfwDestroyWindow(window); }

        glfwTerminate();
    }

    void VulkanGraphicsContext::SetTitle(string title) {
        glfwSetWindowTitle(window, title.c_str());
    }

    bool VulkanGraphicsContext::ShouldClose() {
        return !!glfwWindowShouldClose(window);
    }

    void VulkanGraphicsContext::PrepareWindowView(ecs::View &view) {
        glm::ivec2 scaled = glm::vec2(CVarWindowSize.Get()) * CVarWindowScale.Get();

        if (glfwFullscreen != CVarWindowFullscreen.Get()) {
            if (CVarWindowFullscreen.Get() == 0) {
                glfwSetWindowMonitor(window, nullptr, storedWindowPos.x, storedWindowPos.y, scaled.x, scaled.y, 0);
                glfwFullscreen = 0;
            } else if (CVarWindowFullscreen.Get() == 1) {
                glfwGetWindowPos(window, &storedWindowPos.x, &storedWindowPos.y);
                glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, scaled.x, scaled.y, 60);
                glfwFullscreen = 1;
            }
        } else if (glfwWindowSize != scaled) {
            if (CVarWindowFullscreen.Get()) {
                glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, scaled.x, scaled.y, 60);
            } else {
                glfwSetWindowSize(window, scaled.x, scaled.y);
            }

            glfwWindowSize = scaled;
        }

        view.extents = CVarWindowSize.Get();
        view.fov = glm::radians(CVarFieldOfView.Get());
    }

    const vector<glm::ivec2> &VulkanGraphicsContext::MonitorModes() {
        if (!monitorModes.empty()) return monitorModes;

        int count;
        const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

        for (int i = 0; i < count; i++) {
            glm::ivec2 size(modes[i].width, modes[i].height);
            if (std::find(monitorModes.begin(), monitorModes.end(), size) == monitorModes.end()) {
                monitorModes.push_back(size);
            }
        }

        std::sort(monitorModes.begin(), monitorModes.end(), [](const glm::ivec2 &a, const glm::ivec2 &b) {
            return a.x > b.x || (a.x == b.x && a.y > b.y);
        });

        return monitorModes;
    }

    const glm::ivec2 VulkanGraphicsContext::CurrentMode() {
        const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode != NULL) { return glm::ivec2(mode->width, mode->height); }
        return glm::ivec2(0);
    }

    void VulkanGraphicsContext::SwapBuffers() {
        glfwSwapBuffers(window);
    }

    void VulkanGraphicsContext::BeginFrame() {
        // Do nothing for now1
    }

    void VulkanGraphicsContext::EndFrame() {
        double frameEnd = glfwGetTime();
        fpsTimer += frameEnd - lastFrameEnd;
        frameCounter++;

        if (fpsTimer > 1.0) {
            SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
            frameCounter = 0;
            fpsTimer = 0;
        }

        lastFrameEnd = frameEnd;
    }

    void VulkanGraphicsContext::DisableCursor() {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void VulkanGraphicsContext::EnableCursor() {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    std::shared_ptr<GpuTexture> VulkanGraphicsContext::LoadTexture(shared_ptr<Image> image, bool genMipmap) {
        // TODO
        return nullptr;
    }
} // namespace sp
