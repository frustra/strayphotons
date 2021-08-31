#include "DeviceContext.hh"

#include "CommandContext.hh"
#include "Pipeline.hh"
#include "RenderPass.hh"
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/CFunc.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace sp::vulkan {
    const uint64_t FENCE_WAIT_TIME = 1e10; // nanoseconds, assume deadlock after this time
    const uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_2;

    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                        void *pContext) {
        auto typeStr = vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes));
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            Errorf("Vulkan Error %s: %s", typeStr, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            Logf("Vulkan Warning %s: %s", typeStr, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            Logf("Vulkan Info %s: %s", typeStr, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            Debugf("Vulkan Verbose %s: %s", typeStr, pCallbackData->pMessage);
            break;
        default:
            break;
        }
        return VK_FALSE;
    }

    static void glfwErrorCallback(int error, const char *message) {
        Errorf("GLFW returned %d: %s", error, message);
    }

    DeviceContext::DeviceContext() {
        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit()) { throw "glfw failed"; }

        Assert(glfwVulkanSupported(), "Vulkan not supported");

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

        // Disable OpenGL context creation
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        Logf("Available Vulkan extensions: %u", availableExtensions.size());
        for (auto &ext : availableExtensions) {
            Logf("\t%s", ext.extensionName.data());
        }

        auto availableLayers = vk::enumerateInstanceLayerProperties();
        Logf("Available Vulkan layers: %u", availableLayers.size());
        for (auto &layer : availableLayers) {
            Logf("\t%s %s", layer.layerName.data(), layer.description.data());
        }

        std::vector<const char *> extensions, layers;
        uint32_t requiredExtensionCount = 0;
        auto requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
        for (uint32_t i = 0; i < requiredExtensionCount; i++) {
            extensions.emplace_back(requiredExtensions[i]);
            Logf("Required extension: %s", requiredExtensions[i]);
        }
        extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#if SP_DEBUG
        Logf("Running with vulkan validation layers");
        layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

        // Create window and surface
        auto initialSize = CVarWindowSize.Get();
        window = glfwCreateWindow(initialSize.x, initialSize.y, "STRAY PHOTONS", nullptr, nullptr);
        Assert(window, "glfw window creation failed");

        vk::ApplicationInfo applicationInfo("Stray Photons",
                                            VK_MAKE_VERSION(1, 0, 0),
                                            "Stray Photons",
                                            VK_MAKE_VERSION(1, 0, 0),
                                            VULKAN_API_VERSION);

        vk::InstanceCreateInfo createInfo(vk::InstanceCreateFlags(),
                                          &applicationInfo,
                                          layers.size(),
                                          layers.data(),
                                          extensions.size(),
                                          extensions.data());

        vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
        debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

        debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
#if SP_DEBUG
        debugInfo.messageSeverity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                     vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
#endif
        debugInfo.pfnUserCallback = &VulkanDebugCallback;
        debugInfo.pUserData = this;
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo;

        instance = vk::createInstanceUnique(createInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
        debugMessenger = instance->createDebugUtilsMessengerEXTUnique(debugInfo, nullptr);

        vk::SurfaceKHR glfwSurface;
        auto result = glfwCreateWindowSurface(*instance, window, nullptr, (VkSurfaceKHR *)&glfwSurface);
        AssertVKSuccess(result, "creating window surface");

        vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderDynamic> deleter(*instance);
        surface = vk::UniqueSurfaceKHR(std::move(glfwSurface), deleter);

        auto physicalDevices = instance->enumeratePhysicalDevices();
        // TODO: Prioritize discrete GPUs and check for capabilities like Geometry/Compute shaders
        if (physicalDevices.size() > 0) {
            // TODO: Check device extension support
            auto properties = physicalDevices.front().getProperties();
            // auto features = device.getFeatures();
            Logf("Using graphics device: %s", properties.deviceName);
            physicalDevice = physicalDevices.front();
        }
        Assert(physicalDevice, "No suitable graphics device found!");

        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool hasGraphicsQueueFamily = false;
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsQueueFamily = i;
                hasGraphicsQueueFamily = true;
                break;
            }
        }
        Assert(hasGraphicsQueueFamily, "Couldn't find a Graphics queue family");

        bool hasPresentQueueFamily = false;
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (physicalDevice.getSurfaceSupportKHR(i, *surface)) {
                presentQueueFamily = i;
                hasPresentQueueFamily = true;
                break;
            }
        }
        Assert(hasPresentQueueFamily, "Couldn't find a Present queue family");

        std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily, presentQueueFamily};
        std::vector<vk::DeviceQueueCreateInfo> queueInfos;
        float queuePriority = 1.0f;
        for (auto queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueInfo;
            queueInfo.queueFamilyIndex = queueFamily;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueInfos.push_back(queueInfo);
        }

        vector<const char *> enabledDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                        VK_KHR_MULTIVIEW_EXTENSION_NAME,
                                                        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME};

        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();

        for (auto requiredExtension : enabledDeviceExtensions) {
            bool found = false;
            for (auto &availableExtension : availableDeviceExtensions) {
                if (strncmp(requiredExtension, availableExtension.extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                    found = true;
                    break;
                }
            }
            Assert(found, string("device must have extension ") + requiredExtension);
        }

        vk::PhysicalDeviceFeatures2 deviceFeatures2;
        auto &availableDeviceFeatures = deviceFeatures2.features;

        vk::PhysicalDeviceMultiviewFeatures availableMultiviewFeatures;
        deviceFeatures2.pNext = &availableMultiviewFeatures;

        physicalDevice.getFeatures2KHR(&deviceFeatures2);

        Assert(availableDeviceFeatures.multiViewport, "device must support multiViewport");
        Assert(availableDeviceFeatures.fillModeNonSolid, "device must support fillModeNonSolid");
        Assert(availableDeviceFeatures.wideLines, "device must support wideLines");
        Assert(availableDeviceFeatures.largePoints, "device must support largePoints");
        Assert(availableDeviceFeatures.geometryShader, "device must support geometryShader");
        Assert(availableMultiviewFeatures.multiview, "device must support multiview");
        Assert(availableMultiviewFeatures.multiviewGeometryShader, "device must support multiviewGeometryShader");

        vk::PhysicalDeviceFeatures enabledDeviceFeatures;
        enabledDeviceFeatures.multiViewport = true;
        enabledDeviceFeatures.fillModeNonSolid = true;
        enabledDeviceFeatures.wideLines = true;
        enabledDeviceFeatures.largePoints = true;
        enabledDeviceFeatures.geometryShader = true;

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.queueCreateInfoCount = queueInfos.size();
        deviceInfo.pQueueCreateInfos = queueInfos.data();
        deviceInfo.pEnabledFeatures = &enabledDeviceFeatures;
        deviceInfo.enabledExtensionCount = enabledDeviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
        deviceInfo.enabledLayerCount = layers.size();
        deviceInfo.ppEnabledLayerNames = layers.data();

        device = physicalDevice.createDeviceUnique(deviceInfo, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

        graphicsQueue = device->getQueue(graphicsQueueFamily, 0);
        presentQueue = device->getQueue(presentQueueFamily, 0);

        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        renderThreadCommandPool = device->createCommandPoolUnique(poolInfo);

        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (auto &frame : perFrame) {
            frame.imageAvailableSemaphore = device->createSemaphoreUnique(semaphoreInfo);
            frame.renderCompleteSemaphore = device->createSemaphoreUnique(semaphoreInfo);
            frame.inFlightFence = device->createFenceUnique(fenceInfo);
        }

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VULKAN_API_VERSION;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = *device;
        allocatorInfo.instance = *instance;
        allocatorInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        Assert(vmaCreateAllocator(&allocatorInfo, &allocator) == VK_SUCCESS, "allocator init failed");

        pipelinePool.reset(new PipelineManager(*this));
        renderPassPool.reset(new RenderPassManager(*this));
        framebufferPool.reset(new FramebufferManager(*this));

        funcs.reset(new CFuncCollection);
        funcs->Register("reloadshaders", "Recompile any changed shaders", [&]() {
            ReloadShaders();
        });

        CreateSwapchain();
    }

    DeviceContext::~DeviceContext() {
        if (device) { vkDeviceWaitIdle(*device); }
        if (window) { glfwDestroyWindow(window); }

        glfwTerminate();

        {
            // TODO: these will be allocated from a pool later and not need to be manually cleaned up
            // The image needs to be destroyed before VMA
            depthImageView.reset();
            depthImage.Destroy();
        }

        vmaDestroyAllocator(allocator);
    }

    // Releases old swapchain after creating a new one
    void DeviceContext::CreateSwapchain() {
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        auto presentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        for (auto &mode : presentModes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                presentMode = vk::PresentModeKHR::eMailbox;
                break;
            }
        }

        vk::SurfaceFormatKHR surfaceFormat = surfaceFormats[0];
        for (auto &format : surfaceFormats) {
            if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                if (format.format == vk::Format::eB8G8R8A8Srgb) {
                    surfaceFormat = format;
                    break;
                } else if (format.format == vk::Format::eR8G8B8A8Srgb) {
                    surfaceFormat = format;
                    break;
                }
            }
        }
        Assert(surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear, "surface must support sRGB");

        vk::SwapchainCreateInfoKHR swapchainInfo;
        swapchainInfo.surface = *surface;
        swapchainInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        // TODO: Check capabilities.currentExtent is valid and correctly handles high dpi
        swapchainInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapchainInfo.imageArrayLayers = 1;
        // TODO: use vk::ImageUsageFlagBits::eTransferDst for rendering from another texture
        swapchainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        if (graphicsQueueFamily != presentQueueFamily) {
            // we need to manage image data coherency between queues ourselves if we use concurrent sharing
            /*uint32_t queueFamilyIndices[] = {graphicsQueueFamily, presentQueueFamily};
            swapchainInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapchainInfo.queueFamilyIndexCount = 2;
            swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;*/
            throw "graphics queue and present queue need to be from the same queue family";
        } else {
            swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
        }
        swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
        swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = true;
        swapchainInfo.oldSwapchain = *swapchain;

        auto newSwapchain = device->createSwapchainKHRUnique(swapchainInfo, nullptr);
        perSwapchainImage.clear();
        swapchain.swap(newSwapchain);
        swapchainVersion++;

        auto swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        swapchainExtent = swapchainInfo.imageExtent;
        perSwapchainImage.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            auto &perSCI = perSwapchainImage[i];
            perSCI.image = swapchainImages[i];

            vk::ImageViewCreateInfo createInfo;
            createInfo.image = swapchainImages[i];
            createInfo.viewType = vk::ImageViewType::e2D;
            createInfo.format = swapchainInfo.imageFormat;
            createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            perSCI.imageViewInfo = createInfo;
            perSCI.imageView = device->createImageViewUnique(createInfo);
        }

        depthImageView.reset();

        vk::ImageCreateInfo depthImageInfo;
        depthImageInfo.imageType = vk::ImageType::e2D;
        depthImageInfo.format = vk::Format::eD24UnormS8Uint;
        depthImageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        depthImageInfo.extent = vk::Extent3D(swapchainExtent.width, swapchainExtent.height, 1);
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = vk::SampleCountFlagBits::e1;
        depthImage = AllocateImage(depthImageInfo, VMA_MEMORY_USAGE_GPU_ONLY);

        depthImageViewInfo.format = depthImageInfo.format;
        depthImageViewInfo.image = *depthImage;
        depthImageViewInfo.viewType = vk::ImageViewType::e2D;
        depthImageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        depthImageViewInfo.subresourceRange.baseMipLevel = 0;
        depthImageViewInfo.subresourceRange.levelCount = 1;
        depthImageViewInfo.subresourceRange.baseArrayLayer = 0;
        depthImageViewInfo.subresourceRange.layerCount = 1;
        depthImageView = device->createImageViewUnique(depthImageViewInfo);
    }

    void DeviceContext::RecreateSwapchain() {
        vkDeviceWaitIdle(*device);
        CreateSwapchain();
    }

    void DeviceContext::SetTitle(string title) {
        glfwSetWindowTitle(window, title.c_str());
    }

    bool DeviceContext::ShouldClose() {
        return !!glfwWindowShouldClose(window);
    }

    void DeviceContext::PrepareWindowView(ecs::View &view) {
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

        view.extents = {swapchainExtent.width, swapchainExtent.height};
        view.fov = glm::radians(CVarFieldOfView.Get());
    }

    const vector<glm::ivec2> &DeviceContext::MonitorModes() {
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

    const glm::ivec2 DeviceContext::CurrentMode() {
        const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode != NULL) { return glm::ivec2(mode->width, mode->height); }
        return glm::ivec2(0);
    }

    void DeviceContext::UpdateInputModeFromFocus() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::FocusLock>>();
        if (lock.Has<ecs::FocusLock>()) {
            auto layer = lock.Get<ecs::FocusLock>().PrimaryFocus();
            if (layer == ecs::FocusLayer::GAME) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }

    void DeviceContext::BeginFrame() {
        UpdateInputModeFromFocus();

        auto result = device->waitForFences({*Frame().inFlightFence}, true, FENCE_WAIT_TIME);
        AssertVKSuccess(result, "timed out waiting for fence");

        try {
            auto acquireResult =
                device->acquireNextImageKHR(*swapchain, UINT64_MAX, *Frame().imageAvailableSemaphore, nullptr);
            swapchainImageIndex = acquireResult.value;
        } catch (const vk::OutOfDateKHRError &) {
            RecreateSwapchain();
            return BeginFrame();
        }

        if (SwapchainImage().inFlightFence) {
            result = device->waitForFences({SwapchainImage().inFlightFence}, true, FENCE_WAIT_TIME);
            AssertVKSuccess(result, "timed out waiting for fence");
        }
        SwapchainImage().inFlightFence = *Frame().inFlightFence;

        vmaSetCurrentFrameIndex(allocator, frameCounter);
    }

    void DeviceContext::SwapBuffers() {
        vk::Semaphore renderCompleteSem = *Frame().renderCompleteSemaphore;
        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderCompleteSem;

        vk::SwapchainKHR swapchains[] = {*swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &swapchainImageIndex;

        try {
            auto presentResult = presentQueue.presentKHR(presentInfo);
            if (presentResult == vk::Result::eSuboptimalKHR) RecreateSwapchain();
        } catch (const vk::OutOfDateKHRError &) { RecreateSwapchain(); }

        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void DeviceContext::EndFrame() {
        frameCounter++;
        if (frameCounter == UINT32_MAX) frameCounter = 0;

        double frameEnd = glfwGetTime();
        fpsTimer += frameEnd - lastFrameEnd;
        frameCounterThisSecond++;

        if (fpsTimer > 1.0) {
            SetTitle("STRAY PHOTONS (" + std::to_string(frameCounterThisSecond) + " FPS)");
            frameCounterThisSecond = 0;
            fpsTimer = 0;
        }

        lastFrameEnd = frameEnd;
    }

    shared_ptr<GpuTexture> DeviceContext::LoadTexture(shared_ptr<const Image> image, bool genMipmap) {
        // TODO
        return nullptr;
    }

    vector<vk::UniqueCommandBuffer> DeviceContext::AllocateCommandBuffers(uint32_t count,
                                                                          vk::CommandBufferLevel level) {

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = *renderThreadCommandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = count;

        return device->allocateCommandBuffersUnique(allocInfo);
    }

    CommandContextPtr DeviceContext::GetCommandContext() {
        // TODO: should be queue local, and thread local eventually
        CommandContextPtr cmd;
        auto &freeCmd = Frame().freeCommandContexts;
        if (!freeCmd.empty()) {
            cmd = freeCmd.back();
            freeCmd.pop_back();
        } else {
            cmd = make_shared<CommandContext>(*this);
        }
        cmd->Begin();
        return cmd;
    }

    void DeviceContext::Submit(CommandContextPtr &cmdArg) {
        CommandContextPtr cmd = cmdArg;
        cmdArg.reset();
        cmd->End();

        vk::SubmitInfo submitInfo;
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        auto imageAvailableSem = *Frame().imageAvailableSemaphore;
        auto renderCompleteSem = *Frame().renderCompleteSemaphore;

        const vk::CommandBuffer commandBuffer = cmd->Raw();

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSem;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSem;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        auto frameFence = *Frame().inFlightFence;
        device->resetFences({frameFence});
        graphicsQueue.submit({submitInfo}, frameFence);

        Frame().freeCommandContexts.push_back(cmd);
    }

    UniqueBuffer DeviceContext::AllocateBuffer(vk::DeviceSize size,
                                               vk::BufferUsageFlags usage,
                                               VmaMemoryUsage residency) {
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        return UniqueBuffer(bufferInfo, allocInfo, allocator);
    }

    UniqueImage DeviceContext::AllocateImage(vk::ImageCreateInfo info, VmaMemoryUsage residency) {
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        return UniqueImage(info, allocInfo, allocator);
    }

    ShaderHandle DeviceContext::LoadShader(const string &name) {
        ShaderHandle &handle = shaderHandles[name];
        if (handle) return handle;

        auto shader = shaders.emplace_back(CreateShader(name, {}));
        handle = static_cast<ShaderHandle>(shaders.size());
        return handle;
    }

    shared_ptr<Shader> DeviceContext::CreateShader(const string &name, Hash64 compareHash) {
        auto asset = GAssets.Load("shaders/vulkan/bin/" + name + ".spv");
        if (!asset) {
            Assert(false, "could not load shader: " + name);
            return nullptr;
        }
        asset->WaitUntilValid();

        auto newHash = Hash128To64(asset->Hash());
        if (compareHash == newHash) return nullptr;

        vk::ShaderModuleCreateInfo shaderCreateInfo;
        shaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(asset->Buffer());
        shaderCreateInfo.codeSize = asset->BufferSize();

        auto shaderModule = device->createShaderModuleUnique(shaderCreateInfo);

        auto reflection = spv_reflect::ShaderModule(asset->BufferSize(), asset->Buffer());
        if (reflection.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
            Assert(false, "could not parse shader: " + name + " error: " + std::to_string(reflection.GetResult()));
        }

        return make_shared<Shader>(name, std::move(shaderModule), std::move(reflection), newHash);
    }

    shared_ptr<Shader> DeviceContext::GetShader(ShaderHandle handle) const {
        if (handle == 0 || shaders.size() < (size_t)handle) return nullptr;

        return shaders[handle - 1];
    }

    void DeviceContext::ReloadShaders() {
        for (size_t i = 0; i < shaders.size(); i++) {
            auto &currentShader = shaders[i];
            auto newShader = CreateShader(currentShader->name, currentShader->hash);
            if (newShader) { shaders[i] = newShader; }
        }
    }

    shared_ptr<Pipeline> DeviceContext::GetGraphicsPipeline(const PipelineCompileInput &input) {
        return pipelinePool->GetGraphicsPipeline(input);
    }

    RenderPassInfo DeviceContext::SwapchainRenderPassInfo(bool depth, bool stencil) {
        ImageView view(*SwapchainImage().imageView, SwapchainImage().imageViewInfo, swapchainExtent);
        view.swapchainLayout = vk::ImageLayout::ePresentSrcKHR;

        std::array<float, 4> clearColor = {0.0f, 1.0f, 0.0f, 1.0f};

        RenderPassInfo info;
        info.PushColorAttachment(view, LoadOp::Clear, StoreOp::Store, clearColor);

        if (depth) {
            ImageView depthView(*depthImageView, depthImageViewInfo, swapchainExtent);
            info.SetDepthStencilAttachment(depthView, LoadOp::Clear, StoreOp::DontCare);
        }

        return info;
    }

    shared_ptr<RenderPass> DeviceContext::GetRenderPass(const RenderPassInfo &info) {
        return renderPassPool->GetRenderPass(info);
    }

    shared_ptr<Framebuffer> DeviceContext::GetFramebuffer(const RenderPassInfo &info) {
        return framebufferPool->GetFramebuffer(info);
    }
} // namespace sp::vulkan
