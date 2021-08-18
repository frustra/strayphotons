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
        vk::PhysicalDevice physicalDevice;
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
            if (physicalDevice.getSurfaceSupportKHR(i, *surface)) {
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

        device = physicalDevice.createDeviceUnique(deviceInfo, nullptr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

        graphicsQueue = device->getQueue(graphicsQueueFamily.value(), 0);
        presentQueue = device->getQueue(presentQueueFamily.value(), 0);

        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        auto presentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
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
        swapChain = device->createSwapchainKHRUnique(swapChainInfo, nullptr);

        swapChainImages = device->getSwapchainImagesKHR(*swapChain);
        swapChainImageFormat = swapChainInfo.imageFormat;
        swapChainExtent = swapChainInfo.imageExtent;

        swapChainImageViews.clear();
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            vk::ImageViewCreateInfo createInfo;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = vk::ImageViewType::e2D;
            createInfo.format = swapChainImageFormat;
            createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            swapChainImageViews.emplace_back(device->createImageViewUnique(createInfo));
        }

        /// very temporary code to build a test pipeline

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
        viewport.width = swapChainExtent.width;
        viewport.height = swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.extent = swapChainExtent;

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
        colorAttachment.format = swapChainImageFormat;
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

        swapChainFramebuffers.clear();
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vk::ImageView attachments[] = {*swapChainImageViews[i]};

            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = *renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            swapChainFramebuffers.push_back(device->createFramebufferUnique(framebufferInfo));
        }

        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.queueFamilyIndex = *graphicsQueueFamily;

        commandPool = device->createCommandPoolUnique(poolInfo);

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = *commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = (uint32_t)swapChainFramebuffers.size();

        commandBuffers = device->allocateCommandBuffers(allocInfo);

        for (size_t i = 0; i < commandBuffers.size(); i++) {
            vk::CommandBufferBeginInfo beginInfo;
            auto &commands = commandBuffers[i];
            commands.begin(beginInfo);

            vk::RenderPassBeginInfo renderPassInfo;
            renderPassInfo.renderPass = *renderPass;
            renderPassInfo.framebuffer = *swapChainFramebuffers[i];
            renderPassInfo.renderArea.extent = swapChainExtent;

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

        vk::SemaphoreCreateInfo semaphoreInfo;
        imageAvailableSem = device->createSemaphoreUnique(semaphoreInfo);
        renderCompleteSem = device->createSemaphoreUnique(semaphoreInfo);

        /// end very temporary code
    }

    VulkanGraphicsContext::~VulkanGraphicsContext() {
        if (device) { vkDeviceWaitIdle(*device); }
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
        vk::Semaphore signalSemaphores[] = {*renderCompleteSem};
        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        vk::SwapchainKHR swapChains[] = {*swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        auto presentResult = presentQueue.presentKHR(presentInfo);
        Assert(presentResult == vk::Result::eSuccess, "present");

        // TODO: allow multiple frames in flight to fill GPU, see
        // https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Frames-in-flight
        presentQueue.waitIdle();
    }

    void VulkanGraphicsContext::BeginFrame() {
        imageIndex = device->acquireNextImageKHR(*swapChain, UINT64_MAX, *imageAvailableSem, nullptr);

        vk::SubmitInfo submitInfo;

        vk::Semaphore waitSemaphores[] = {*imageAvailableSem};
        vk::Semaphore signalSemaphores[] = {*renderCompleteSem};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        graphicsQueue.submit({submitInfo});
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
