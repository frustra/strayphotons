#include "VulkanGraphicsContext.hh"

#include "core/Logging.hh"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// temporary for shader access, shaders should be compiled somewhere else later
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace sp {
    const int MAX_FRAMES_IN_FLIGHT = 2;
    const int FENCE_WAIT_TIME = 1e10; // nanoseconds, assume deadlock after this time

    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                        void *pGraphicsContext) {
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

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

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
                                            VK_API_VERSION_1_2);
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
        Assert(result == VK_SUCCESS, "Failed to create window surface");

        vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderDynamic> deleter(*instance);
        surface = vk::UniqueSurfaceKHR(std::move(glfwSurface), deleter);

        auto physicalDevices = instance->enumeratePhysicalDevices();
        // TODO: Prioritize discrete GPUs and check for capabilities like Geometry/Compute shaders
        for (auto &dev : physicalDevices) {
            // TODO: Check device extension support
            auto properties = dev.getProperties();
            // auto features = device.getFeatures();
            Logf("Using graphics device: %s", properties.deviceName);
            physicalDevice = dev;
            break;
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
                                                        VK_KHR_MULTIVIEW_EXTENSION_NAME};

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

        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores.push_back(std::move(device->createSemaphoreUnique(semaphoreInfo)));
            renderCompleteSemaphores.push_back(std::move(device->createSemaphoreUnique(semaphoreInfo)));
            inFlightFences.push_back(std::move(device->createFenceUnique(fenceInfo)));
        }

        CreateSwapchain();
        CreateTestPipeline();
    }

    VulkanGraphicsContext::~VulkanGraphicsContext() {
        if (device) { vkDeviceWaitIdle(*device); }
        if (window) { glfwDestroyWindow(window); }

        glfwTerminate();
    }

    // Releases old swapchain after creating a new one
    void VulkanGraphicsContext::CreateSwapchain() {
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
        swapchainImageViews.clear();
        swapchain.swap(std::move(newSwapchain));

        swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        swapchainImageFormat = swapchainInfo.imageFormat;
        swapchainExtent = swapchainInfo.imageExtent;

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            vk::ImageViewCreateInfo createInfo;
            createInfo.image = swapchainImages[i];
            createInfo.viewType = vk::ImageViewType::e2D;
            createInfo.format = swapchainImageFormat;
            createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            swapchainImageViews.emplace_back(device->createImageViewUnique(createInfo));
        }

        imagesInFlight.resize(swapchainImages.size());
    }

    void VulkanGraphicsContext::CreateTestPipeline() {
        // very temporary code to build a test pipeline

        auto vertShaderAsset = GAssets.Load("shaders/vulkan/bin/test.vert.spv");
        vk::ShaderModuleCreateInfo vertShaderCreateInfo;
        vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(vertShaderAsset->CharBuffer());
        vertShaderCreateInfo.codeSize = vertShaderAsset->buffer.size();
        auto vertShaderModule = device->createShaderModuleUnique(vertShaderCreateInfo);

        auto fragShaderAsset = GAssets.Load("shaders/vulkan/bin/test.frag.spv");
        vk::ShaderModuleCreateInfo fragShaderCreateInfo;
        fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(fragShaderAsset->CharBuffer());
        fragShaderCreateInfo.codeSize = fragShaderAsset->buffer.size();
        auto fragShaderModule = device->createShaderModuleUnique(fragShaderCreateInfo);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = *vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = *fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

        vk::Viewport viewport;
        viewport.width = swapchainExtent.width;
        viewport.height = swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.extent = swapchainExtent;

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace = vk::FrontFace::eClockwise;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::AttachmentDescription colorAttachment;
        colorAttachment.format = swapchainImageFormat;
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        renderPass = device->createRenderPassUnique(renderPassInfo);

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = *pipelineLayout;
        pipelineInfo.renderPass = *renderPass;
        pipelineInfo.subpass = 0;

        auto pipelinesResult = device->createGraphicsPipelinesUnique({}, {pipelineInfo});
        Assert(pipelinesResult.result == vk::Result::eSuccess, "creating pipelines");
        graphicsPipeline = std::move(pipelinesResult.value[0]);

        swapchainFramebuffers.clear();
        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            vk::ImageView attachments[] = {*swapchainImageViews[i]};

            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = *renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapchainExtent.width;
            framebufferInfo.height = swapchainExtent.height;
            framebufferInfo.layers = 1;

            swapchainFramebuffers.push_back(device->createFramebufferUnique(framebufferInfo));
        }

        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        commandPool = device->createCommandPoolUnique(poolInfo);

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = *commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = (uint32_t)swapchainFramebuffers.size();

        commandBuffers = device->allocateCommandBuffers(allocInfo);

        for (size_t i = 0; i < commandBuffers.size(); i++) {
            vk::CommandBufferBeginInfo beginInfo;
            auto &commands = commandBuffers[i];
            commands.begin(beginInfo);

            vk::RenderPassBeginInfo renderPassInfo;
            renderPassInfo.renderPass = *renderPass;
            renderPassInfo.framebuffer = *swapchainFramebuffers[i];
            renderPassInfo.renderArea.extent = swapchainExtent;

            vk::ClearColorValue color(std::array{0.0f, 1.0f, 0.0f, 1.0f});
            vk::ClearValue clearColor(color);
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            commands.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
            commands.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
            commands.draw(3, 1, 0, 0);
            commands.endRenderPass();
            commands.end();
        }
    }

    void VulkanGraphicsContext::RecreateSwapchain() {
        vkDeviceWaitIdle(*device);

        // Created by CreateTestPipeline
        swapchainFramebuffers.clear();
        commandBuffers.clear();
        graphicsPipeline.reset();
        pipelineLayout.reset();
        renderPass.reset();

        CreateSwapchain();
        CreateTestPipeline();
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

    void VulkanGraphicsContext::BeginFrame() {
        auto fence = *inFlightFences[currentFrame];
        auto result = device->waitForFences({fence}, true, FENCE_WAIT_TIME);
        Assert(result == vk::Result::eSuccess, "timed out waiting for fence");

        auto imageAvailableSem = *imageAvailableSemaphores[currentFrame];
        auto renderCompleteSem = *renderCompleteSemaphores[currentFrame];

        try {
            auto acquireResult = device->acquireNextImageKHR(*swapchain, UINT64_MAX, imageAvailableSem, nullptr);
            imageIndex = acquireResult.value;
        } catch (vk::OutOfDateKHRError) {
            RecreateSwapchain();
            return BeginFrame();
        }

        if (imagesInFlight[imageIndex]) {
            result = device->waitForFences({imagesInFlight[imageIndex]}, true, FENCE_WAIT_TIME);
            Assert(result == vk::Result::eSuccess, "timed out waiting for fence");
        }
        imagesInFlight[imageIndex] = *inFlightFences[currentFrame];

        vk::SubmitInfo submitInfo;
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSem;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSem;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        device->resetFences({fence});
        graphicsQueue.submit({submitInfo}, fence);
    }

    void VulkanGraphicsContext::SwapBuffers() {
        vk::Semaphore renderCompleteSem = *renderCompleteSemaphores[currentFrame];
        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderCompleteSem;

        vk::SwapchainKHR swapchains[] = {*swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        try {
            auto presentResult = presentQueue.presentKHR(presentInfo);
            if (presentResult == vk::Result::eSuboptimalKHR) RecreateSwapchain();
        } catch (vk::OutOfDateKHRError) { RecreateSwapchain(); }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
