#include "DeviceContext.hh"

#include "CommandContext.hh"
#include "Pipeline.hh"
#include "RenderPass.hh"
#include "RenderTarget.hh"
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Image.hh"
#include "core/CFunc.hh"
#include "core/Logging.hh"
#include "core/StackVector.hh"
#include "ecs/EcsImpl.hh"

#include <algorithm>
#include <iostream>
#include <new>
#include <optional>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <glfw/glfw3native.h>
#endif

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace sp::vulkan {
    const uint64_t FENCE_WAIT_TIME = 1e10; // nanoseconds, assume deadlock after this time
    const uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_2;

    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                        void *pContext) {
        auto deviceContext = static_cast<DeviceContext *>(pContext);
        if (messageType & deviceContext->disabledDebugMessages) return VK_FALSE;

        auto typeStr = vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageType));
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

    void DeviceContext::DeleteAllocator(VmaAllocator alloc) {
        if (alloc) vmaDestroyAllocator(alloc);
    }

    DeviceContext::DeviceContext(bool enableValidationLayers) : allocator(nullptr, DeleteAllocator) {
        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit()) { throw "glfw failed"; }

        Assert(glfwVulkanSupported(), "Vulkan not supported");

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

        // Disable OpenGL context creation
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        std::vector<const char *> extensions, layers;
        bool hasMemoryRequirements2Ext = false, hasDedicatedAllocationExt = false;

        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        Debugf("Available Vulkan extensions: %u", availableExtensions.size());
        for (auto &ext : availableExtensions) {
            string_view name(ext.extensionName.data());
            Debugf("\t%s", name);

            if (name == VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) {
                hasMemoryRequirements2Ext = true;
            } else if (name == VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) {
                hasDedicatedAllocationExt = true;
            } else {
                continue;
            }
            extensions.push_back(name.data());
        }

        auto availableLayers = vk::enumerateInstanceLayerProperties();
        Debugf("Available Vulkan layers: %u", availableLayers.size());
        for (auto &layer : availableLayers) {
            Debugf("\t%s %s", layer.layerName.data(), layer.description.data());
        }

        uint32_t requiredExtensionCount = 0;
        auto requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
        for (uint32_t i = 0; i < requiredExtensionCount; i++) {
            extensions.emplace_back(requiredExtensions[i]);
        }
        extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        if (enableValidationLayers) {
            Logf("Running with Vulkan validation layer");
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

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
#ifdef SP_DEBUG
        debugInfo.messageSeverity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
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

        std::array<uint32, QUEUE_TYPES_COUNT> queueIndex;
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        vector<uint32> queuesUsedCount(queueFamilies.size());
        vector<vector<float>> queuePriority(queueFamilies.size());

        const auto findQueue = [&](QueueType queueType,
                                   vk::QueueFlags require,
                                   vk::QueueFlags deny,
                                   float priority,
                                   bool surfaceSupport = false) {
            for (uint32_t i = 0; i < queueFamilies.size(); i++) {
                auto &props = queueFamilies[i];
                if (!(props.queueFlags & require)) continue;
                if (props.queueFlags & deny) continue;
                if (surfaceSupport && !physicalDevice.getSurfaceSupportKHR(i, *surface)) continue;
                if (queuesUsedCount[i] >= props.queueCount) continue;

                queueFamilyIndex[queueType] = i;
                queueIndex[queueType] = queuesUsedCount[i]++;
                queuePriority[i].push_back(priority);
                return true;
            }
            return false;
        };

        if (!findQueue(QUEUE_TYPE_GRAPHICS, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute, {}, 1.0f, true))
            Abort("could not find a supported graphics queue family");

        if (!findQueue(QUEUE_TYPE_COMPUTE, vk::QueueFlagBits::eCompute, {}, 0.5f)) {
            // must be only one queue that supports compute, fall back to it
            queueFamilyIndex[QUEUE_TYPE_COMPUTE] = queueFamilyIndex[QUEUE_TYPE_GRAPHICS];
            queueIndex[QUEUE_TYPE_COMPUTE] = queueIndex[QUEUE_TYPE_GRAPHICS];
        }

        if (!findQueue(QUEUE_TYPE_TRANSFER,
                       vk::QueueFlagBits::eTransfer,
                       vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute,
                       0.3f)) {
            // no queues support only transfer, fall back to a compute queue that also supports transfer
            if (!findQueue(QUEUE_TYPE_TRANSFER, vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eGraphics, 0.3f)) {
                // fall back to the main compute queue
                queueFamilyIndex[QUEUE_TYPE_TRANSFER] = queueFamilyIndex[QUEUE_TYPE_COMPUTE];
                queueIndex[QUEUE_TYPE_TRANSFER] = queueIndex[QUEUE_TYPE_COMPUTE];
            }
        }

        imageTransferGranularity = queueFamilies[queueFamilyIndex[QUEUE_TYPE_TRANSFER]].minImageTransferGranularity;
        Assert(imageTransferGranularity.depth <= 1, "transfer queue doesn't support 2D images");

        std::vector<vk::DeviceQueueCreateInfo> queueInfos;
        for (uint32 i = 0; i < queueFamilies.size(); i++) {
            if (queuesUsedCount[i] == 0) continue;

            vk::DeviceQueueCreateInfo queueInfo;
            queueInfo.queueFamilyIndex = i;
            queueInfo.queueCount = queuesUsedCount[i];
            queueInfo.pQueuePriorities = queuePriority[i].data();
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

        Assert(availableDeviceFeatures.fillModeNonSolid, "device must support fillModeNonSolid");
        Assert(availableDeviceFeatures.wideLines, "device must support wideLines");
        Assert(availableDeviceFeatures.largePoints, "device must support largePoints");
        Assert(availableDeviceFeatures.samplerAnisotropy, "device must support anisotropic sampling");
        Assert(availableMultiviewFeatures.multiview, "device must support multiview");
        Assert(availableMultiviewFeatures.multiviewGeometryShader, "device must support multiviewGeometryShader");

        vk::PhysicalDeviceFeatures2 enabledDeviceFeatures2;
        auto &enabledDeviceFeatures = enabledDeviceFeatures2.features;
        enabledDeviceFeatures.fillModeNonSolid = true;
        enabledDeviceFeatures.wideLines = true;
        enabledDeviceFeatures.largePoints = true;
        enabledDeviceFeatures.samplerAnisotropy = true;
        enabledDeviceFeatures2.pNext = &availableMultiviewFeatures;

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.queueCreateInfoCount = queueInfos.size();
        deviceInfo.pQueueCreateInfos = queueInfos.data();
        deviceInfo.pNext = &enabledDeviceFeatures2;
        deviceInfo.enabledExtensionCount = enabledDeviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
        deviceInfo.enabledLayerCount = layers.size();
        deviceInfo.ppEnabledLayerNames = layers.data();

        device = physicalDevice.createDeviceUnique(deviceInfo, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

        for (uint32 queueType = 0; queueType < QUEUE_TYPES_COUNT; queueType++) {
            queues[queueType] = device->getQueue(queueFamilyIndex[queueType], queueIndex[queueType]);
        }

        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (auto &frame : frameContexts) {
            frame.imageAvailableSemaphore = device->createSemaphoreUnique(semaphoreInfo);
            frame.renderCompleteSemaphore = device->createSemaphoreUnique(semaphoreInfo);
            frame.inFlightFence = device->createFenceUnique(fenceInfo);

            for (uint32 queueType = 0; queueType < QUEUE_TYPES_COUNT; queueType++) {
                vk::CommandPoolCreateInfo poolInfo;
                poolInfo.queueFamilyIndex = queueFamilyIndex[queueType];
                poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
                frame.commandContexts[queueType].commandPool = device->createCommandPoolUnique(poolInfo);
            }
        }

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VULKAN_API_VERSION;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = *device;
        allocatorInfo.instance = *instance;
        allocatorInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        if (hasMemoryRequirements2Ext && hasDedicatedAllocationExt) {
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        }

        VmaAllocator alloc;
        Assert(vmaCreateAllocator(&allocatorInfo, &alloc) == VK_SUCCESS, "allocator init failed");
        allocator.reset(alloc);

        renderTargetPool = make_unique<RenderTargetManager>(*this);
        pipelinePool = make_unique<PipelineManager>(*this);
        renderPassPool = make_unique<RenderPassManager>(*this);
        framebufferPool = make_unique<FramebufferManager>(*this);

        funcs = make_unique<CFuncCollection>();
        funcs->Register("reloadshaders", "Recompile any changed shaders", [&]() {
            ReloadShaders();
        });

        CreateSwapchain();
    }

    DeviceContext::~DeviceContext() {
        if (device) { device->waitIdle(); }
        if (window) { glfwDestroyWindow(window); }

        glfwTerminate();
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
                if (format.format == vk::Format::eR8G8B8A8Srgb) {
                    surfaceFormat = format;
                    break;
                } else if (format.format == vk::Format::eB8G8R8A8Srgb) {
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
        swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
        swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
        swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = true;
        swapchainInfo.oldSwapchain = *swapchain;

        auto newSwapchain = device->createSwapchainKHRUnique(swapchainInfo, nullptr);
        swapchainImageContexts.clear();
        swapchain.swap(newSwapchain);

        auto swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        swapchainExtent = swapchainInfo.imageExtent;
        swapchainImageContexts.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            ImageViewCreateInfo imageViewInfo;
            imageViewInfo.image = make_shared<Image>(swapchainImages[i], swapchainInfo.imageFormat, swapchainExtent);
            imageViewInfo.swapchainLayout = vk::ImageLayout::ePresentSrcKHR;
            swapchainImageContexts[i].imageView = CreateImageView(imageViewInfo);
        }
    }

    void DeviceContext::RecreateSwapchain() {
        device->waitIdle();
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
        view.UpdateProjectionMatrix();
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
        PrepareResourcesForFrame();
        vmaSetCurrentFrameIndex(allocator.get(), frameCounter);
    }

    void DeviceContext::PrepareResourcesForFrame() {
        for (auto &pool : Frame().commandContexts) {
            // Resets all command buffers in the pool, so they can be recorded and used again.
            if (pool.nextIndex > 0) device->resetCommandPool(*pool.commandPool);
            pool.nextIndex = 0;
        }

        erase_if(Frame().inFlightBuffers, [&](auto &buf) {
            return device->getFenceStatus(*buf.fence) == vk::Result::eSuccess;
        });

        for (auto &pool : Frame().bufferPools) {
            erase_if(pool, [&](auto &buf) {
                if (!buf.used) return true;
                buf.used = false;
                return false;
            });
        }

        renderTargetPool->TickFrame();
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
            auto presentResult = queues[QUEUE_TYPE_GRAPHICS].presentKHR(presentInfo);
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

    CommandContextPtr DeviceContext::GetCommandContext(CommandContextType type) {
        // TODO(multithread): should segregate command contexts by thread
        CommandContextPtr cmd;
        auto &pool = Frame().commandContexts[QueueType(type)];
        if (pool.nextIndex < pool.list.size()) {
            cmd = pool.list[pool.nextIndex++];

            // Reset cmd to default state
            auto buffer = std::move(cmd->RawRef());
            cmd->~CommandContext();
            new (cmd.get()) CommandContext(*this, std::move(buffer), type);
        } else {
            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.commandPool = *pool.commandPool;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandBufferCount = 1;
            auto buffers = device->allocateCommandBuffersUnique(allocInfo);

            cmd = make_shared<CommandContext>(*this, std::move(buffers[0]), type);
            pool.list.push_back(cmd);
            pool.nextIndex++;
        }
        cmd->Begin();
        return cmd;
    }

    void DeviceContext::Submit(CommandContextPtr &cmdArg,
                               vk::ArrayProxy<const vk::Semaphore> signalSemaphores,
                               vk::ArrayProxy<const vk::Semaphore> waitSemaphores,
                               vk::ArrayProxy<const vk::PipelineStageFlags> waitStages,
                               vk::Fence fence) {
        CommandContextPtr cmd = cmdArg;
        // Invalidate caller's reference, this CommandContext is unusable until a subsequent frame.
        cmdArg.reset();
        cmd->End();

        Assert(waitSemaphores.size() == waitStages.size(), "must have exactly one wait stage per wait semaphore");

        StackVector<vk::Semaphore, 8> signalSemArray;
        signalSemArray.push(signalSemaphores.data(), signalSemaphores.size());

        StackVector<vk::Semaphore, 8> waitSemArray;
        waitSemArray.push(waitSemaphores.data(), waitSemaphores.size());

        StackVector<vk::PipelineStageFlags, 8> waitStageArray;
        waitStageArray.push(waitStages.data(), waitStages.size());

        if (cmd->WritesToSwapchain()) {
            waitStageArray.push(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            waitSemArray.push(*Frame().imageAvailableSemaphore);
            signalSemArray.push(*Frame().renderCompleteSemaphore);
        }

        const vk::CommandBuffer commandBuffer = cmd->Raw();

        vk::SubmitInfo submitInfo;
        submitInfo.waitSemaphoreCount = waitSemArray.size();
        submitInfo.pWaitSemaphores = waitSemArray.data();
        submitInfo.pWaitDstStageMask = waitStageArray.data();
        submitInfo.signalSemaphoreCount = signalSemArray.size();
        submitInfo.pSignalSemaphores = signalSemArray.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (cmd->WritesToSwapchain()) {
            Assert(!fence, "can't use custom fence on submission to swapchain");
            fence = *Frame().inFlightFence;
            device->resetFences({fence});
        }

        auto &queue = queues[QueueType(cmd->GetType())];
        queue.submit({submitInfo}, fence);

        for (auto &func : cmd->afterSubmitFuncs) {
            func();
        }
        cmd->afterSubmitFuncs.clear();
    }

    BufferPtr DeviceContext::AllocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage residency) {
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        return make_shared<Buffer>(bufferInfo, allocInfo, allocator.get());
    }

    BufferPtr DeviceContext::GetFramePooledBuffer(BufferType type, vk::DeviceSize size) {
        auto &pool = Frame().bufferPools[type];
        for (auto &buf : pool) {
            if (!buf.used && buf.size == size) {
                buf.used = true;
                return buf.buffer;
            }
        }

        vk::BufferUsageFlags usage;
        VmaMemoryUsage residency;
        switch (type) {
        case BUFFER_TYPE_UNIFORM:
            usage = vk::BufferUsageFlagBits::eUniformBuffer;
            residency = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        default:
            Abort("unknown buffer type " + std::to_string(type));
        }

        Debugf("Allocating buffer type %d with size %d", type, size);
        auto buffer = AllocateBuffer(size, usage, residency);
        pool.emplace_back(PooledBuffer{buffer, size, true});
        return buffer;
    }

    ImagePtr DeviceContext::AllocateImage(const vk::ImageCreateInfo &info, VmaMemoryUsage residency) {
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        return make_shared<Image>(info, allocInfo, allocator.get());
    }

    ImagePtr DeviceContext::CreateImage(vk::ImageCreateInfo createInfo,
                                        const uint8 *srcData,
                                        size_t srcDataSize,
                                        bool genMipmap) {
        if (!createInfo.mipLevels) createInfo.mipLevels = genMipmap ? CalculateMipmapLevels(createInfo.extent) : 1;
        if (!createInfo.arrayLayers) createInfo.arrayLayers = 1;

        if (!srcData || srcDataSize == 0) {
            Assert(!genMipmap, "must pass initial data to generate a mipmap");
            return AllocateImage(createInfo, VMA_MEMORY_USAGE_GPU_ONLY);
        }

        Assert(createInfo.arrayLayers == 1, "can't load initial data into an image array");
        Assert(!genMipmap || createInfo.mipLevels > 1, "can't generate mipmap for a single level image");

        createInfo.usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        auto image = AllocateImage(createInfo, VMA_MEMORY_USAGE_GPU_ONLY);

        auto stagingBuf =
            CreateBuffer(srcData, srcDataSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU);

        auto transferCmd = GetCommandContext(CommandContextType::TransferAsync);

        transferCmd->ImageBarrier(image,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::PipelineStageFlagBits::eTopOfPipe,
                                  {},
                                  vk::PipelineStageFlagBits::eTransfer,
                                  vk::AccessFlagBits::eTransferWrite);

        vk::BufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = FormatToAspectFlags(createInfo.format);
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = createInfo.extent;

        transferCmd->Raw().copyBufferToImage(*stagingBuf, *image, vk::ImageLayout::eTransferDstOptimal, {region});

        ImageBarrierInfo transferToGeneral;
        transferToGeneral.trackImageLayout = false;
        transferToGeneral.srcQueueFamilyIndex = QueueFamilyIndex(CommandContextType::TransferAsync),
        transferToGeneral.dstQueueFamilyIndex = QueueFamilyIndex(CommandContextType::General);
        if (genMipmap) transferToGeneral.mipLevelCount = 1;

        transferCmd->ImageBarrier(
            image,
            vk::ImageLayout::eTransferDstOptimal,
            genMipmap ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::AccessFlagBits::eTransferWrite,
            transferToGeneral);

        auto transferComplete = GetEmptySemaphore();
        Frame().inFlightBuffers.emplace_back(InFlightBuffer{stagingBuf, device->createFenceUnique({})});
        Submit(transferCmd, {transferComplete}, {}, {}, *Frame().inFlightBuffers.back().fence);

        auto graphicsCmd = GetCommandContext();

        if (!genMipmap) {
            transferToGeneral.trackImageLayout = true;
            graphicsCmd->ImageBarrier(image,
                                      vk::ImageLayout::eTransferDstOptimal,
                                      vk::ImageLayout::eShaderReadOnlyOptimal,
                                      vk::PipelineStageFlagBits::eFragmentShader,
                                      {},
                                      vk::PipelineStageFlagBits::eFragmentShader,
                                      vk::AccessFlagBits::eShaderRead,
                                      transferToGeneral);

            Submit(graphicsCmd, {}, {transferComplete}, {vk::PipelineStageFlagBits::eFragmentShader});
            return image;
        }

        graphicsCmd->ImageBarrier(image,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  vk::PipelineStageFlagBits::eTransfer,
                                  {},
                                  vk::PipelineStageFlagBits::eTransfer,
                                  vk::AccessFlagBits::eTransferRead,
                                  transferToGeneral);

        ImageBarrierInfo transferMips;
        transferMips.trackImageLayout = false;
        transferMips.baseMipLevel = 1;
        transferMips.mipLevelCount = createInfo.mipLevels - 1;

        graphicsCmd->ImageBarrier(image,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::PipelineStageFlagBits::eTransfer,
                                  {},
                                  vk::PipelineStageFlagBits::eTransfer,
                                  vk::AccessFlagBits::eTransferWrite,
                                  transferMips);

        vk::Offset3D currentExtent = {(int32)createInfo.extent.width,
                                      (int32)createInfo.extent.height,
                                      (int32)createInfo.extent.depth};

        transferMips.mipLevelCount = 1;

        for (uint32 i = 1; i < createInfo.mipLevels; i++) {
            auto prevMipExtent = currentExtent;
            currentExtent.x = std::max(currentExtent.x >> 1, 1);
            currentExtent.y = std::max(currentExtent.y >> 1, 1);
            currentExtent.z = std::max(currentExtent.z >> 1, 1);

            vk::ImageBlit blit;
            blit.srcSubresource = {vk::ImageAspectFlagBits::eColor, i - 1, 0, 1};
            blit.srcOffsets[0] = vk::Offset3D();
            blit.srcOffsets[1] = prevMipExtent;
            blit.dstSubresource = {vk::ImageAspectFlagBits::eColor, i, 0, 1};
            blit.dstOffsets[0] = vk::Offset3D();
            blit.dstOffsets[1] = currentExtent;

            graphicsCmd->Raw().blitImage(*image,
                                         vk::ImageLayout::eTransferSrcOptimal,
                                         *image,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         {blit},
                                         vk::Filter::eLinear);

            transferMips.baseMipLevel = i;
            graphicsCmd->ImageBarrier(image,
                                      vk::ImageLayout::eTransferDstOptimal,
                                      vk::ImageLayout::eTransferSrcOptimal,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      {},
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::AccessFlagBits::eTransferWrite,
                                      transferMips);
        }

        // Each mip has now been transitioned to TransferSrc.
        image->SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);

        graphicsCmd->ImageBarrier(image,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  vk::ImageLayout::eShaderReadOnlyOptimal,
                                  vk::PipelineStageFlagBits::eTransfer,
                                  vk::AccessFlagBits::eTransferWrite,
                                  vk::PipelineStageFlagBits::eFragmentShader,
                                  vk::AccessFlagBits::eShaderRead);

        Submit(graphicsCmd, {}, {transferComplete}, {vk::PipelineStageFlagBits::eTransfer});
        return image;
    }

    ImageViewPtr DeviceContext::CreateImageView(ImageViewCreateInfo info) {
        if (info.format == vk::Format::eUndefined) info.format = info.image->Format();

        if (info.arrayLayerCount == VK_REMAINING_ARRAY_LAYERS)
            info.arrayLayerCount = info.image->ArrayLayers() - info.baseArrayLayer;

        if (info.mipLevelCount == VK_REMAINING_MIP_LEVELS)
            info.mipLevelCount = info.image->MipLevels() - info.baseMipLevel;

        vk::ImageViewCreateInfo createInfo;
        createInfo.image = *info.image;
        createInfo.format = info.format;
        createInfo.viewType = info.viewType;
        createInfo.components = info.mapping;
        createInfo.subresourceRange.aspectMask = FormatToAspectFlags(info.format);
        createInfo.subresourceRange.baseMipLevel = info.baseMipLevel;
        createInfo.subresourceRange.levelCount = info.mipLevelCount;
        createInfo.subresourceRange.baseArrayLayer = info.baseArrayLayer;
        createInfo.subresourceRange.layerCount = info.arrayLayerCount;
        return make_shared<ImageView>(device->createImageViewUnique(createInfo), info);
    }

    ImageViewPtr DeviceContext::CreateImageAndView(const vk::ImageCreateInfo &imageInfo,
                                                   ImageViewCreateInfo viewInfo,
                                                   const uint8 *srcData,
                                                   size_t srcDataSize,
                                                   bool genMipmap) {
        viewInfo.image = CreateImage(imageInfo, srcData, srcDataSize, genMipmap);
        return CreateImageView(viewInfo);
    }

    shared_ptr<GpuTexture> DeviceContext::LoadTexture(shared_ptr<const sp::Image> image, bool genMipmap) {
        image->WaitUntilValid();

        vk::ImageCreateInfo createInfo;
        createInfo.extent = vk::Extent3D(image->GetWidth(), image->GetHeight(), 1);
        Assert(createInfo.extent.width > 0 && createInfo.extent.height > 0, "image has zero size");

        createInfo.format = FormatFromTraits(image->GetComponents(), 8, true);
        Assert(createInfo.format != vk::Format::eUndefined, "invalid image format");

        const uint8_t *data = image->GetImage().get();
        Assert(data, "missing image data");

        ImageViewCreateInfo viewInfo;
        viewInfo.defaultSampler = GetSampler(SamplerType::TrilinearTiled);
        return CreateImageAndView(createInfo, viewInfo, data, image->ByteSize(), genMipmap);
    }

    vk::Sampler DeviceContext::GetSampler(SamplerType type) {
        auto &sampler = namedSamplers[type];
        if (sampler) return *sampler;

        vk::SamplerCreateInfo samplerInfo;

        switch (type) {
        case SamplerType::BilinearClamp:
        case SamplerType::BilinearTiled:
        case SamplerType::TrilinearClamp:
        case SamplerType::TrilinearTiled:
            samplerInfo.magFilter = vk::Filter::eLinear;
            samplerInfo.minFilter = vk::Filter::eLinear;
            break;
        case SamplerType::NearestClamp:
        case SamplerType::NearestTiled:
            samplerInfo.magFilter = vk::Filter::eNearest;
            samplerInfo.minFilter = vk::Filter::eNearest;
            break;
        }

        switch (type) {
        case SamplerType::TrilinearClamp:
        case SamplerType::TrilinearTiled:
            samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
            samplerInfo.maxAnisotropy = 4.0f;
            samplerInfo.anisotropyEnable = true;
            samplerInfo.minLod = 0;
            samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            break;
        default:
            samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
        }

        switch (type) {
        case SamplerType::BilinearTiled:
        case SamplerType::TrilinearTiled:
        case SamplerType::NearestTiled:
            samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
            samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
            samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
            break;
        default:
            samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        }

        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        sampler = device->createSamplerUnique(samplerInfo);
        return *sampler;
    }

    vk::Sampler DeviceContext::GetSampler(const vk::SamplerCreateInfo &info) {
        Assert(info.pNext == 0, "sampler info pNext can't be set");

        SamplerKey key(info);
        auto &sampler = adhocSamplers[key];
        if (sampler) return *sampler;

        sampler = device->createSamplerUnique(info);
        return *sampler;
    }

    RenderTargetPtr DeviceContext::GetRenderTarget(const RenderTargetDesc &desc) {
        return renderTargetPool->Get(desc);
    }

    ShaderHandle DeviceContext::LoadShader(const string &name) {
        ShaderHandle &handle = shaderHandles[name];
        if (handle) return handle;

        auto shader = shaders.emplace_back(CreateShader(name, {}));
        handle = static_cast<ShaderHandle>(shaders.size());
        return handle;
    }

    shared_ptr<Shader> DeviceContext::CreateShader(const string &name, Hash64 compareHash) {
        auto asset = GAssets.Load("shaders/vulkan/bin/" + name + ".spv", compareHash != Hash64());
        Assert(asset, "could not load shader: " + name);
        asset->WaitUntilValid();

        auto newHash = Hash128To64(asset->Hash());
        if (compareHash == newHash) return nullptr;

        vk::ShaderModuleCreateInfo shaderCreateInfo;
        shaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(asset->Buffer());
        shaderCreateInfo.codeSize = asset->BufferSize();

        auto shaderModule = device->createShaderModuleUnique(shaderCreateInfo);

        auto reflection = spv_reflect::ShaderModule(asset->BufferSize(), asset->Buffer());
        if (reflection.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
            Abort("could not parse shader: " + name + " error: " + std::to_string(reflection.GetResult()));
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

    ImageViewPtr DeviceContext::SwapchainImageView() {
        return SwapchainImage().imageView;
    }

    shared_ptr<RenderPass> DeviceContext::GetRenderPass(const RenderPassInfo &info) {
        return renderPassPool->GetRenderPass(info);
    }

    shared_ptr<Framebuffer> DeviceContext::GetFramebuffer(const RenderPassInfo &info) {
        return framebufferPool->GetFramebuffer(info);
    }

    vk::Semaphore DeviceContext::GetEmptySemaphore() {
        vk::SemaphoreCreateInfo semCreateInfo;
        semaphores.push_back(device->createSemaphoreUnique(semCreateInfo));
        return *semaphores.back();
    }

    void *DeviceContext::Win32WindowHandle() {
#ifdef _WIN32
        return glfwGetWin32Window(GetWindow());
#else
        return nullptr;
#endif
    }
} // namespace sp::vulkan
