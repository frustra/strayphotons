/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "common/ObjectPool.hh"
#include "common/Tracing.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "magic_enum.hpp"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/EnumTypes.hh"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <thread>
#include <vulkan/vulkan_core.h>
#define VULKAN_HPP_USE_REFLECT

#include "DeviceContext.hh"
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Image.hh"
#include "console/CFunc.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/components/GuiElement.hh"
#include "game/Game.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/core/GraphicsManager.hh"
#include "graphics/vulkan/Compositor.hh"
#include "graphics/vulkan/ProfilerGui.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"
#include "graphics/vulkan/core/Pipeline.hh"
#include "graphics/vulkan/core/RenderPass.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/core/VkTracing.hh"
#include "gui/MenuGuiManager.hh"
#include "strayphotons/Async.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"
#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <cxxopts.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

void WriteReflection(const spv_reflect::ShaderModule &obj, bool flatten_cbuffers, std::ostream &os);

namespace sp::vulkan {
    const uint64_t FENCE_WAIT_TIME = 1e10; // nanoseconds, assume deadlock after this time
    const uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_2;

#if VK_HEADER_VERSION >= 304
    static VkBool32 VulkanDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::Flags<vk::DebugUtilsMessageTypeFlagBitsEXT> messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pContext) {
        auto deviceContext = static_cast<DeviceContext *>(pContext);
        if (messageType & deviceContext->disabledDebugMessages) return VK_FALSE;

        auto typeStr = vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageType));
        std::string message(pCallbackData->pMessage);

        switch (messageSeverity) {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
    #ifdef TRACY_ENABLE_GRAPHICS
            // Ignore Tracy timer query errors
            if (message.find("CoreValidation-DrawState-QueryNotReset") != std::string_view::npos) break;
    #endif
            if (message.find("(subresource: aspectMask 0x1 array layer 0, mip level 0) to be in layout "
                             "VK_IMAGE_LAYOUT_GENERAL--instead, current layout is VK_IMAGE_LAYOUT_PREINITIALIZED.") !=
                std::string_view::npos)
                break;
            Errorf("VK %s %s", typeStr, message);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) break;
            Warnf("VK %s %s", typeStr, message);
            break;
        default:
            break;
        }
        Tracef("VK %s %s", typeStr, message);
        return VK_FALSE;
    }
#else
    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pContext) {
        auto deviceContext = static_cast<DeviceContext *>(pContext);
        if (messageType & deviceContext->disabledDebugMessages) return VK_FALSE;

        auto typeStr = vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageType));
        std::string message(pCallbackData->pMessage);

        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    #ifdef TRACY_ENABLE_GRAPHICS
            if (message.find("CoreValidation-DrawState-QueryNotReset") != std::string_view::npos) {
                // Ignore Tracy timer query errors
                return VK_FALSE;
            }
    #endif
            if (message.find("(subresource: aspectMask 0x1 array layer 0, mip level 0) to be in layout "
                             "VK_IMAGE_LAYOUT_GENERAL--instead, current layout is VK_IMAGE_LAYOUT_PREINITIALIZED.") !=
                std::string_view::npos)
                return VK_FALSE;
            Errorf("VK %s %s", typeStr, message);
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
                Tracef("VK %s %s", typeStr, message);
            } else {
                Warnf("VK %s %s", typeStr, message);
            }
        } else {
            Tracef("VK %s %s", typeStr, message);
        }
        return VK_FALSE;
    }
#endif

    DeviceContext::DeviceContext(Game &game)
        : game(game), graphics(*game.graphics), mainThread(std::this_thread::get_id()), allocator(nullptr, nullptr),
          threadContexts(32), frameBeginQueue("BeginFrame", 0, {}), frameEndQueue("EndFrame", 0, {}),
          allocatorQueue("GPUAllocator", 0, {}), submitQueue("GPUSubmit", 1, std::chrono::milliseconds(1)),
          graph(*this) {
        ZoneScoped;

        auto submitThreadInit = submitQueue.Dispatch<void>(NewDispatchSource, [this] {
            submitThread = std::this_thread::get_id();
        });

        try {
            Assertf(graphics.vkInstance, "GraphicsManager has no Vulkan instance set.");

            bool enableSwapchain = true;
            if (graphics.glfwWindow) {
                Logf("Graphics starting up (Vulkan GLFW)");
                Assertf(graphics.vkSurface, "GraphicsManager has no Vulkan surface set.");
            } else if (graphics.winitContext) {
                Logf("Graphics starting up (Vulkan Winit)");
                Assertf(graphics.vkSurface, "GraphicsManager has no Vulkan surface set.");
            } else {
                Logf("Graphics starting up (Vulkan Headless)");
                enableSwapchain = false;
            }

            VULKAN_HPP_DEFAULT_DISPATCHER.init(&vkGetInstanceProcAddr);

            instance = graphics.vkInstance.get();
            surface = graphics.vkSurface.get();

            VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

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

            debugMessenger = instance.createDebugUtilsMessengerEXTUnique(debugInfo);

            bool hasMemoryRequirements2Ext = false, hasDedicatedAllocationExt = false;
            auto availableExtensions = vk::enumerateInstanceExtensionProperties();
            // Debugf("Available Vulkan extensions: %u", availableExtensions.size());
            for (auto &ext : availableExtensions) {
                std::string_view name(ext.extensionName.data());
                // Debugf("\t%s", name);

                if (name == VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) {
                    hasMemoryRequirements2Ext = true;
                } else if (name == VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) {
                    hasDedicatedAllocationExt = true;
                }
            }

            Logf("Available graphics devices:");
            vk::PhysicalDeviceType selectedType = vk::PhysicalDeviceType::eOther;
            auto physicalDevices = instance.enumeratePhysicalDevices();
            std::vector<vk::PhysicalDeviceProperties2> physicalDeviceProperties(physicalDevices.size());
            for (uint32_t i = 0; i < physicalDevices.size(); i++) {
                auto &device = physicalDevices[i];
                auto &deviceProperties = physicalDeviceProperties[i];
                device.getProperties2(&deviceProperties);
                Logf("  device %u: [%u:%u] %s",
                    i,
                    deviceProperties.properties.vendorID,
                    deviceProperties.properties.deviceID,
                    deviceProperties.properties.deviceName.data());
            }
            uint32_t selectedDeviceIndex = ~0u;
            if (game.options.count("gpu")) {
                auto gpuStr = game.options["gpu"].as<std::string>();
                Logf("Requesting GPU %s", gpuStr);
                std::istringstream ss(gpuStr);
                if (sp::contains(gpuStr, ':')) {
                    uint32_t vendorID = ~0u;
                    char divider = 0;
                    uint32_t deviceID = ~0u;
                    ss >> vendorID >> divider >> deviceID;
                    for (uint32_t i = 0; i < physicalDevices.size(); i++) {
                        auto &deviceProperties = physicalDeviceProperties[i];
                        if (deviceProperties.properties.vendorID == vendorID &&
                            deviceProperties.properties.deviceID == deviceID) {
                            selectedDeviceIndex = i;
                            break;
                        }
                    }
                    if (selectedDeviceIndex >= physicalDevices.size()) {
                        Errorf("Requested GPU [%u:%u] not found", vendorID, deviceID);
                    }
                } else {
                    ss >> selectedDeviceIndex;
                    if (selectedDeviceIndex >= physicalDevices.size()) {
                        Errorf("Requested GPU %u not found", selectedDeviceIndex);
                    }
                }
            }
            if (selectedDeviceIndex < physicalDevices.size()) {
                selectedDevice = physicalDevices[selectedDeviceIndex];
                selectedDeviceProperties = physicalDeviceProperties[selectedDeviceIndex];
                selectedType = physicalDeviceProperties[selectedDeviceIndex].properties.deviceType;
            } else {
                for (uint32_t i = 0; i < physicalDevices.size(); i++) {
                    auto &device = physicalDevices[i];
                    auto &deviceProperties = physicalDeviceProperties[i];
                    // TODO: Check device extension support
                    // TODO: Check for capabilities like Geometry/Compute shaders
                    // auto features = device.getFeatures();
                    auto deviceType = deviceProperties.properties.deviceType;
                    if (deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                        selectedDevice = device;
                        selectedDeviceProperties = deviceProperties;
                        selectedType = deviceProperties.properties.deviceType;
                        selectedDeviceIndex = i;
                        break;
                    } else if (deviceType == vk::PhysicalDeviceType::eIntegratedGpu && deviceType != selectedType) {
                        selectedDevice = device;
                        selectedDeviceProperties = deviceProperties;
                        selectedType = deviceProperties.properties.deviceType;
                        selectedDeviceIndex = i;
                    }
                }
            }
            Assert(selectedDevice, "No suitable graphics device found!");
            Logf("Using graphics device %u: [%u:%u] %s",
                selectedDeviceIndex,
                selectedDeviceProperties.properties.vendorID,
                selectedDeviceProperties.properties.deviceID,
                selectedDeviceProperties.properties.deviceName.data());

            auto queueFamilies = selectedDevice.getQueueFamilyProperties();
            std::vector<std::vector<float>> allocatedQueuePriorities(queueFamilies.size());

            const auto findQueue = [&](QueueID queueId,
                                       vk::QueueFlags require,
                                       vk::QueueFlags deny,
                                       float priority,
                                       bool surfaceSupport = false) {
                Queue &queue = queues[queueId];
                for (uint32_t i = 0; i < queueFamilies.size(); i++) {
                    auto &props = queueFamilies[i];
                    if (!(props.queueFlags & require)) continue;
                    if (props.queueFlags & deny) continue;
                    if (surfaceSupport && !selectedDevice.getSurfaceSupportKHR(i, surface)) continue;
                    if (allocatedQueuePriorities[i].size() >= props.queueCount) continue;

                    queue.queueFamilyIndex = i;
                    queue.indexInFamily = allocatedQueuePriorities[i].size();
                    allocatedQueuePriorities[i].emplace_back(priority);
                    Logf("Queue Family %s: %u", queueId, i);
                    return true;
                }
                return false;
            };

            if (!findQueue(QueueID::General,
                    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute,
                    {},
                    1.0f,
                    enableSwapchain))
                Abortf("could not find a supported graphics queue family");

            if (!findQueue(QueueID::ComputeAsync, vk::QueueFlagBits::eCompute, {}, 0.5f)) {
                // must be only one queue that supports compute, fall back to it
                queues[QueueID::ComputeAsync] = queues[QueueID::General];
            }

            if (!findQueue(QueueID::TransferAsync,
                    vk::QueueFlagBits::eTransfer,
                    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute,
                    0.3f)) {
                // no queues support only transfer, fall back to a compute queue that also supports transfer
                if (!findQueue(QueueID::TransferAsync,
                        vk::QueueFlagBits::eTransfer,
                        vk::QueueFlagBits::eGraphics,
                        0.3f)) {
                    // fall back to the main compute queue
                    queues[QueueID::TransferAsync] = queues[QueueID::ComputeAsync];
                }
            }

            std::vector<vk::DeviceQueueCreateInfo> queueInfos;
            for (uint32_t i = 0; i < queueFamilies.size(); i++) {
                if (allocatedQueuePriorities[i].empty()) continue;

                vk::DeviceQueueCreateInfo &queueInfo = queueInfos.emplace_back();
                queueInfo.queueFamilyIndex = i;
                queueInfo.queueCount = allocatedQueuePriorities[i].size();
                queueInfo.pQueuePriorities = allocatedQueuePriorities[i].data();
            }

            std::vector<const char *> enabledDeviceExtensions = {
                VK_KHR_MULTIVIEW_EXTENSION_NAME,
                VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
                VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
                VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
            };

            if (enableSwapchain) {
                enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }

            std::array<const char *, 2> optionalDeviceExtensions = {
                VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
                "VK_KHR_portability_subset",
            };

            auto availableDeviceExtensions = selectedDevice.enumerateDeviceExtensionProperties();

            for (auto requiredExtension : enabledDeviceExtensions) {
                bool found = false;
                for (auto &availableExtension : availableDeviceExtensions) {
                    if (strncmp(requiredExtension, availableExtension.extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                        found = true;
                        break;
                    }
                }
                Assertf(found, "device must have extension %s", requiredExtension);
            }
            for (auto optionalExtension : optionalDeviceExtensions) {
                for (auto &availableExtension : availableDeviceExtensions) {
                    if (strncmp(optionalExtension, availableExtension.extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                        enabledDeviceExtensions.push_back(optionalExtension);
                        break;
                    }
                }
            }

            vk::PhysicalDeviceVulkan12Features availableVulkan12Features = {};
            vk::PhysicalDeviceVulkan11Features availableVulkan11Features = {};
            availableVulkan11Features.pNext = &availableVulkan12Features;
            vk::PhysicalDeviceFeatures2 availableDeviceFeatures2 = {};
            availableDeviceFeatures2.pNext = &availableVulkan11Features;
            auto &availableVulkan10Features = availableDeviceFeatures2.features;
            selectedDevice.getFeatures2KHR(&availableDeviceFeatures2);

            vk::PhysicalDeviceVulkan12Features enabledVulkan12Features = {};
            vk::PhysicalDeviceVulkan11Features enabledVulkan11Features = {};
            enabledVulkan11Features.pNext = &enabledVulkan12Features;
            vk::PhysicalDeviceFeatures2 enabledDeviceFeatures2 = {};
            enabledDeviceFeatures2.pNext = &enabledVulkan11Features;
            auto &enabledVulkan10Features = enabledDeviceFeatures2.features;

#define REQUIRE_DEVICE_FEATURE(MAJOR_VERSION, MINOR_VERSION, NAME)                                                     \
    if (availableVulkan##MAJOR_VERSION##MINOR_VERSION##Features.NAME) {                                                \
        enabledVulkan##MAJOR_VERSION##MINOR_VERSION##Features.NAME = true;                                             \
    } else {                                                                                                           \
        Errorf("Graphics Device is missing required Vulkan %s.%s feature: %s", #MAJOR_VERSION, #MINOR_VERSION, #NAME); \
    }

            REQUIRE_DEVICE_FEATURE(1, 2, shaderOutputViewportIndex);
            REQUIRE_DEVICE_FEATURE(1, 2, shaderOutputLayer);
            REQUIRE_DEVICE_FEATURE(1, 2, drawIndirectCount);
            REQUIRE_DEVICE_FEATURE(1, 2, runtimeDescriptorArray);
            REQUIRE_DEVICE_FEATURE(1, 2, descriptorBindingPartiallyBound);
            REQUIRE_DEVICE_FEATURE(1, 2, descriptorBindingVariableDescriptorCount);
            REQUIRE_DEVICE_FEATURE(1, 2, shaderSampledImageArrayNonUniformIndexing);
            REQUIRE_DEVICE_FEATURE(1, 2, descriptorBindingUpdateUnusedWhilePending);

            REQUIRE_DEVICE_FEATURE(1, 1, storageBuffer16BitAccess);
            REQUIRE_DEVICE_FEATURE(1, 1, uniformAndStorageBuffer16BitAccess);
            REQUIRE_DEVICE_FEATURE(1, 1, multiview);
            REQUIRE_DEVICE_FEATURE(1, 1, shaderDrawParameters);

            REQUIRE_DEVICE_FEATURE(1, 0, dualSrcBlend);
            REQUIRE_DEVICE_FEATURE(1, 0, fillModeNonSolid);
            REQUIRE_DEVICE_FEATURE(1, 0, samplerAnisotropy);
            REQUIRE_DEVICE_FEATURE(1, 0, multiDrawIndirect);
            REQUIRE_DEVICE_FEATURE(1, 0, drawIndirectFirstInstance);
            REQUIRE_DEVICE_FEATURE(1, 0, multiViewport);
            REQUIRE_DEVICE_FEATURE(1, 0, shaderInt16);
            // TODO: Change geometry_warp.vert to a compute shader
            REQUIRE_DEVICE_FEATURE(1, 0, vertexPipelineStoresAndAtomics);
            REQUIRE_DEVICE_FEATURE(1, 0, fragmentStoresAndAtomics);
            REQUIRE_DEVICE_FEATURE(1, 0, wideLines);

            vk::DeviceCreateInfo deviceInfo;
            deviceInfo.queueCreateInfoCount = queueInfos.size();
            deviceInfo.pQueueCreateInfos = queueInfos.data();
            deviceInfo.pNext = &enabledDeviceFeatures2;
            deviceInfo.enabledExtensionCount = enabledDeviceExtensions.size();
            deviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

            device = selectedDevice.createDeviceUnique(deviceInfo, nullptr);
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

            for (const QueueID &queueId : magic_enum::enum_values<QueueID>()) {
                Queue &queue = queues[queueId];
                queue.queue = device->getQueue(queue.queueFamilyIndex, queue.indexInFamily);
                queue.lastSubmit = 0;

#ifdef TRACY_ENABLE_GRAPHICS
                if (queueId == QueueID::TransferAsync) continue;

                vk::CommandPoolCreateInfo cmdPoolInfo;
                cmdPoolInfo.queueFamilyIndex = queue.queueFamilyIndex;
                cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
                tracing.cmdPools.push_back(device->createCommandPoolUnique(cmdPoolInfo));

                vk::CommandBufferAllocateInfo cmdAllocInfo;
                cmdAllocInfo.commandPool = *tracing.cmdPools.back();
                cmdAllocInfo.level = vk::CommandBufferLevel::ePrimary;
                cmdAllocInfo.commandBufferCount = 1;
                auto cmdBufs = device->allocateCommandBuffersUnique(cmdAllocInfo);
                tracing.cmdBuffers.insert(tracing.cmdBuffers.end(),
                    std::make_move_iterator(cmdBufs.begin()),
                    std::make_move_iterator(cmdBufs.end()));

                tracing.tracyContexts[queueId] = tracy::CreateVkContext(selectedDevice,
                    *device,
                    queue.queue,
                    *tracing.cmdBuffers.back(),
                    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
                    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetCalibratedTimestampsEXT);
#endif
            }

            vk::SemaphoreCreateInfo semaphoreInfo = {};
            vk::FenceCreateInfo fenceInfo = {};

            for (auto &frame : frameContexts) {
                frame.imageAvailableSemaphore = device->createSemaphoreUnique(semaphoreInfo);
                frame.inFlightFence = std::make_shared<vk::UniqueFence>(device->createFenceUnique(fenceInfo));

                vk::CommandPoolCreateInfo poolInfo;
                poolInfo.queueFamilyIndex = queues[QueueID::General].queueFamilyIndex;
                poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient |
                                 vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
                frame.commandPool = device->createCommandPoolUnique(poolInfo);
            }

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.vulkanApiVersion = VULKAN_API_VERSION;
            allocatorInfo.physicalDevice = selectedDevice;
            allocatorInfo.device = *device;
            allocatorInfo.instance = instance;
            allocatorInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
            allocatorInfo.preferredLargeHeapBlockSize = 1024ull * 1024 * 1024;
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

            if (hasMemoryRequirements2Ext && hasDedicatedAllocationExt) {
                allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
            }

            VmaAllocator alloc;
            Assert(vmaCreateAllocator(&allocatorInfo, &alloc) == VK_SUCCESS, "allocator init failed");
            allocator = std::unique_ptr<VmaAllocator_T, void (*)(VmaAllocator)>(alloc, [](VmaAllocator alloc) {
                if (alloc) vmaDestroyAllocator(alloc);
            });

            pipelinePool = std::make_unique<PipelineManager>(*this);
            renderPassPool = std::make_unique<RenderPassManager>(*this);
            framebufferPool = std::make_unique<FramebufferManager>(*this);
            semaphorePool = std::make_unique<ObjectPool<vk::UniqueSemaphore>>([this](vk::UniqueSemaphore *semaphore) {
                vk::SemaphoreCreateInfo createInfo = {};
                new (semaphore) vk::UniqueSemaphore(device->createSemaphoreUnique(createInfo));
            });
            semaphorePool->Reserve(512);

            for (auto &threadContextUnique : threadContexts) {
                threadContextUnique = std::make_unique<ThreadContext>();
                ThreadContext *threadContext = threadContextUnique.get();

                threadContext->bufferPool = std::make_unique<BufferPool>(*this);

                for (const QueueID &queueId : magic_enum::enum_values<QueueID>()) {
                    Queue &queue = queues[queueId];
                    ThreadContext::Queue &threadQueue = threadContext->queues[queueId];
                    vk::CommandPoolCreateInfo poolInfo;
                    poolInfo.queueFamilyIndex = queue.queueFamilyIndex;
                    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient |
                                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
                    threadQueue.commandPool = device->createCommandPoolUnique(poolInfo);

                    threadQueue.commandContexts = std::make_unique<ObjectPool<CommandContext>>(
                        [this, threadContext, queueId](CommandContext *cmd) {
                            new (cmd) CommandContext(*this,
                                *threadContext->queues[queueId].commandPool,
                                queueId,
                                CommandContextType::Fence);
                        },
                        [this](CommandContext &cmd) {
                            auto queueId = cmd.GetQueueID();
                            auto buffer = std::move(cmd.RawRef());
                            auto fence = std::move(cmd.fence);
                            if (fence) {
                                Assertf(fence.use_count() == 1,
                                    "CommandContext Fence still in use on reset: %ld",
                                    fence.use_count());
                                Assertf(device->getFenceStatus(fence->get()) == vk::Result::eSuccess,
                                    "CommandContext destroyed before fence completed");
                            }
                            auto acquiredImages = std::move(cmd.acquiredImages);
                            auto acquiredBuffers = std::move(cmd.acquiredBuffers);
                            auto activeImageViews = std::move(cmd.activeImageViews);
                            auto waitSemaphores = std::move(cmd.waitSemaphores);
                            auto signalSemaphores = std::move(cmd.signalSemaphores);
                            auto waitStages = std::move(cmd.waitStages);
                            acquiredImages.clear();
                            acquiredBuffers.clear();
                            activeImageViews.clear();
                            waitSemaphores.clear();
                            signalSemaphores.clear();
                            waitStages.clear();

                            cmd.~CommandContext();
                            new (&cmd) CommandContext(*this, std::move(buffer), queueId, CommandContextType::Fence);
                            cmd.fence = std::move(fence);
                            cmd.acquiredImages = std::move(acquiredImages);
                            cmd.acquiredBuffers = std::move(acquiredBuffers);
                            cmd.activeImageViews = std::move(activeImageViews);
                            cmd.waitSemaphores = std::move(waitSemaphores);
                            cmd.signalSemaphores = std::move(signalSemaphores);
                            cmd.waitStages = std::move(waitStages);

                            cmd.Raw().reset();
                        });
                }
            }

            funcs = std::make_unique<CFuncCollection>();
            funcs->Register("reloadshaders", "Recompile any changed shaders", [&]() {
                reloadShaders = true;
            });
            funcs->Register("vkbufferstats", "Print Vulkan buffer pool stats", [&]() {
                for (auto &tc : threadContexts) {
                    tc->printBufferStats = true;
                }
            });

            perfTimer.reset(new PerfTimer(*this));

            if (graphics.windowHandlers.get_video_modes) {
                size_t modeCount;
                graphics.windowHandlers.get_video_modes(&graphics, &modeCount, nullptr);
                monitorModes.resize(modeCount);
                std::vector<sp_video_mode_t> videoModes(modeCount);
                graphics.windowHandlers.get_video_modes(&graphics, &modeCount, videoModes.data());
                for (size_t i = 0; i < modeCount; i++) {
                    monitorModes[i] = {videoModes[i].width, videoModes[i].height};
                }
                std::sort(monitorModes.begin(), monitorModes.end(), [](const glm::ivec2 &a, const glm::ivec2 &b) {
                    uint8_t ratioBitsA = 0;
                    uint8_t ratioBitsB = 0;
                    if (IsAspect(a, 16, 9)) ratioBitsA |= 1 << 2;
                    if (IsAspect(a, 16, 10)) ratioBitsA |= 1 << 1;
                    if (IsAspect(a, 4, 3)) ratioBitsA |= 1 << 0;
                    if (IsAspect(b, 16, 9)) ratioBitsB |= 1 << 2;
                    if (IsAspect(b, 16, 10)) ratioBitsB |= 1 << 1;
                    if (IsAspect(b, 4, 3)) ratioBitsB |= 1 << 0;
                    if (ratioBitsA != ratioBitsB) {
                        return ratioBitsA > ratioBitsB;
                    } else {
                        return a.x > b.x || (a.x == b.x && a.y > b.y);
                    }
                });
                monitorModes.erase(std::unique(monitorModes.begin(), monitorModes.end()), monitorModes.end());
            }

            if (enableSwapchain) CreateSwapchain();

            compositor = std::make_unique<Compositor>(*this, graph);
        } catch (const vk::InitializationFailedError &err) {
            Errorf("Device initialization failed! %s", err.what());
            deviceResetRequired = true;
        }
        submitThreadInit->Get();
    }

    DeviceContext::~DeviceContext() {
        if (vkRenderer) vkRenderer.reset();
        if (device && !deviceResetRequired) WaitIdle(NewDispatchSource);
#ifdef TRACY_ENABLE_GRAPHICS
        for (auto ctx : tracing.tracyContexts)
            if (ctx) TracyVkDestroy(ctx);
#endif
        Debugf("Destroying DeviceContext");
        swapchain.reset();
        graphics.glfwWindow.reset();
    }

    // Releases old swapchain after creating a new one
    void DeviceContext::CreateSwapchain() {
        ZoneScoped;
        auto surfaceCapabilities = selectedDevice.getSurfaceCapabilitiesKHR(surface);

        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
            return;
        }
        if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max() ||
            surfaceCapabilities.currentExtent.height == std::numeric_limits<uint32_t>::max()) {
            auto windowSize = outputExtents;
            if (outputExtents.x <= 1 || outputExtents.y <= 1) windowSize = CVarWindowSize.Get();
            surfaceCapabilities.currentExtent.width = windowSize.x;
            surfaceCapabilities.currentExtent.height = windowSize.y;
        }

        auto surfaceFormats = selectedDevice.getSurfaceFormatsKHR(surface);
        auto presentModes = selectedDevice.getSurfacePresentModesKHR(surface);

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eImmediate;
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
                }
            }
        }
        Assert(surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear, "surface must support sRGB");

        vk::SwapchainCreateInfoKHR swapchainInfo;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = std::max(surfaceCapabilities.minImageCount, MAX_FRAMES_IN_FLIGHT);
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainInfo.imageExtent.width = glm::clamp(surfaceCapabilities.currentExtent.width,
            surfaceCapabilities.minImageExtent.width,
            surfaceCapabilities.maxImageExtent.width);
        swapchainInfo.imageExtent.height = glm::clamp(surfaceCapabilities.currentExtent.height,
            surfaceCapabilities.minImageExtent.height,
            surfaceCapabilities.maxImageExtent.height);
        swapchainInfo.imageArrayLayers = 1;
        // TODO: use vk::ImageUsageFlagBits::eTransferDst for rendering from another texture
        swapchainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
        swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
        swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = true;
        swapchainInfo.oldSwapchain = *swapchain;

        Tracef("Creating swapchain: %u x %u", swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height);

        auto newSwapchain = device->createSwapchainKHRUnique(swapchainInfo);
        swapchainImageContexts.clear();
        swapchain.swap(newSwapchain);

        auto swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        swapchainImageContexts.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            ImageViewCreateInfo imageViewInfo = {};
            imageViewInfo.image = Image::New(swapchainImages[i], swapchainInfo.imageFormat, swapchainInfo.imageExtent);
            imageViewInfo.swapchainLayout = vk::ImageLayout::ePresentSrcKHR;
            swapchainImageContexts[i].imageView = CreateImageView(imageViewInfo);
            vk::SemaphoreCreateInfo semaphoreInfo = {};
            swapchainImageContexts[i].submitCompleteSemaphore = device->createSemaphoreUnique(semaphoreInfo);
        }
    }

    void DeviceContext::SetOutputExtents(uint32_t width, uint32_t height) {
        outputExtents = glm::uvec2(width, height);
    }

    void DeviceContext::RecreateSwapchain() {
        ZoneScoped;
        Assertf(swapchain, "DeviceContext::RecreateSwapchain called without existing swapchain");
        WaitIdle(NewDispatchSource);
        CreateSwapchain();
    }

    void DeviceContext::InitRenderer(Game &game) {
        renderThread = std::this_thread::get_id();
        vkRenderer = std::make_shared<vulkan::Renderer>(game, *this, graph, *compositor);
    }

    void DeviceContext::Shutdown() {
        vkRenderer.reset();
        compositor.reset();
    }

    std::shared_ptr<Renderer> DeviceContext::GetRenderer() const {
        return vkRenderer;
    }

    GenericCompositor &DeviceContext::GetCompositor() {
        return *compositor;
    }

    AsyncPtr<Gltf> DeviceContext::GenerateMesh(const ecs::VoxelData &voxelData) {
        if (!vkRenderer) {
            Errorf("Cannot generate mesh without active renderer");
            return nullptr;
        }
        return vkRenderer->GenerateMesh(voxelData);
    }

    void DeviceContext::RenderFrame(chrono_clock::duration elapsedTime) {
        if (vkRenderer) vkRenderer->RenderFrame(elapsedTime);
    }

    void DeviceContext::AttachWindow(const std::shared_ptr<GuiContext> &context) {
        if (!context) return;
        auto *perfTimer = GetPerfTimer();
        if (perfTimer) {
            if (!profilerGui) profilerGui = std::make_shared<ProfilerGui>(*perfTimer);
            context->Attach(std::static_pointer_cast<ecs::GuiDefinition>(profilerGui));
        }
        if (vkRenderer) vkRenderer->AttachWindow(context);
    }

    bool DeviceContext::BeginFrame() {
        ZoneScoped;
        Assert(std::this_thread::get_id() == renderThread, "BeginFrame must be called from render thread");

        try {
            if (perfTimer) perfTimer->StartFrame();

            if (reloadShaders.exchange(false)) {
                for (size_t i = 0; i < shaders.size(); i++) {
                    auto &currentShader = shaders[i];
                    auto newShader = CreateShader(currentShader->name, currentShader->hash);
                    if (newShader) {
                        shaders[i] = newShader;
                    }
                }
            }

            auto &fence = Frame().inFlightFence;
            if (Frame().fenceInUse) {
                ZoneScopedN("WaitForFrameFence");
                auto result = device->waitForFences({fence->get()}, true, FENCE_WAIT_TIME);
                AssertVKSuccess(result, "timed out waiting for fence");
                device->resetFences({fence->get()});
                Frame().fenceInUse = false;
            }

            if (swapchain) {
                try {
                    ZoneScopedN("AcquireNextImage");
                    auto acquireResult = device->acquireNextImageKHR(*swapchain,
                        FENCE_WAIT_TIME,
                        *Frame().imageAvailableSemaphore,
                        nullptr);
                    if (acquireResult.result == vk::Result::eTimeout) {
                        Warnf("vkAcquireNextImageKHR timeout");
                        return false;
                    } else if (acquireResult.result == vk::Result::eSuboptimalKHR) {
                        RecreateSwapchain(); // X11 / Wayland returns this on resize
                        return false;
                    } else {
                        AssertVKSuccess(acquireResult.result, "invalid swap chain acquire image");
                    }
                    swapchainImageIndex = acquireResult.value;
                    ZoneValue(swapchainImageIndex);
                } catch (const vk::OutOfDateKHRError &) {
                    RecreateSwapchain(); // Windows returns this on resize
                    return false;
                } catch (const std::system_error &err) {
                    Abortf("Exception: %s", err.what());
                }
            }

            vmaSetCurrentFrameIndex(allocator.get(), frameCounter);
            PrepareResourcesForFrame();

#ifdef TRACY_ENABLE_GRAPHICS
            for (const QueueID &queueId : magic_enum::enum_values<QueueID>()) {
                if (queueId == QueueID::TransferAsync) continue;
                auto prevQueueSubmitFrame = queues[queueId].lastSubmit;
                if (prevQueueSubmitFrame < frameCounter - 1) continue;

                auto trctx = tracing.tracyContexts[queueId];
                if (!trctx) continue;

                auto ctx = GetFencedCommandContext(queueId);
                TracyVkCollect(trctx, ctx->Raw());
                Submit(NewDispatchSource, ctx);

                queues[queueId].lastSubmit = prevQueueSubmitFrame;
            }
#endif

            erase_if(pendingFences, [this](auto &pair) {
                if (!pair.first || pair.first->Ready()) {
                    return true;
                } else if (device->getFenceStatus(pair.second->get()) == vk::Result::eSuccess) {
                    pair.first->Set(nullptr);
                    return true;
                } else {
                    return false;
                }
            });
            frameBeginQueue.Flush();

            Assertf(fence.use_count() == 1, "Frame Fence still in use on reset: %ld", fence.use_count());
        } catch (const vk::DeviceLostError &err) {
            Errorf("Device lost! BeginFrame() %s", err.what());
            deviceResetRequired = true;
            return false;
        }
        return true;
    }

    void DeviceContext::PrepareResourcesForFrame() {
        ZoneScoped;
        // Resets all command buffers in the pool, so they can be recorded and used again.
        if (Frame().nextIndex > 0) {
            ZoneScopedN("ResetCommandPool");
            device->resetCommandPool(*Frame().commandPool);
            Frame().nextIndex = 0;
        }

        Thread().ReleaseAvailableResources();
    }

    void DeviceContext::SwapBuffers() {
        if (!swapchain) return;
        ZoneScoped;

        vk::PresentInfoKHR presentInfo = {};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &SwapchainImage().submitCompleteSemaphore.get();

        vk::SwapchainKHR swapchains[] = {*swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &swapchainImageIndex;

        bool requiresNewSwapchain = false;
        AsyncPtr<void> presentComplete = submitQueue.Dispatch<void>(NewDispatchSource,
            [this, presentInfo, &requiresNewSwapchain] {
                ZoneScopedN("PresentKHR");
                try {
                    auto presentResult = queues[QueueID::General].queue.presentKHR(presentInfo);
                    auto swapchainView = SwapchainImageView();
                    vk::Extent3D swapchainExtent = swapchainView ? swapchainView->Extent() : vk::Extent3D();
                    bool extentsMatch = swapchainExtent.width == outputExtents.x &&
                                        swapchainExtent.height == outputExtents.y;
                    if (presentResult == vk::Result::eSuboptimalKHR || !extentsMatch) requiresNewSwapchain = true;
                } catch (const vk::OutOfDateKHRError &) {
                    requiresNewSwapchain = true;
                }
            });
        presentComplete->Get();
        if (requiresNewSwapchain) RecreateSwapchain();
    }

    void DeviceContext::EndFrame() {
        if (vkRenderer) vkRenderer->EndFrame();

        allocatorQueue.Dispatch<void>(NewDispatchSource, [this]() {
            Thread().ReleaseAvailableResources();
        });

        FlushMainQueue(false, std::chrono::milliseconds(5));

        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

        frameCounter++;
        if (frameCounter == UINT32_MAX) frameCounter = 0;

        double frameEnd = (double)chrono_clock::now().time_since_epoch().count() / 1e9;
        fpsTimer += frameEnd - lastFrameEnd;
        frameCounterThisSecond++;

        if (fpsTimer > 1.0) {
            measuredFrameRate = frameCounterThisSecond;
            frameCounterThisSecond = 0;
            fpsTimer = 0;
        }

        lastFrameEnd = frameEnd;
        if (perfTimer) perfTimer->EndFrame();
    };

    void DeviceContext::WaitIdle(const DispatchSourceInfo &sourceInfo) {
        ZoneScoped;
        if (!deviceResetRequired) {
            auto waitComplete = submitQueue.Dispatch<void>(sourceInfo, [this] {
                device->waitIdle();
            });
            waitComplete->Get();
        }
    }

    void DeviceContext::FlushMainQueue(bool blockUntilReady, chrono_clock::duration timeLimit) {
        ZoneScoped;
        ZoneValue(blockUntilReady);
        bool allReady = false;
        if (timeLimit.count() == 0) {
            do {
                allReady = true;
                if (!allocatorQueue.Flush(false)) allReady = false;
                if (!frameEndQueue.Flush(false)) allReady = false;
            } while (blockUntilReady && !allReady);
        } else {
            chrono_clock::time_point start = chrono_clock::now();
            do {
                allReady = true;
                auto elapsed = chrono_clock::now() - start;
                if (elapsed >= timeLimit) break;
                if (!allocatorQueue.Flush(false, timeLimit - elapsed)) allReady = false;
                elapsed = chrono_clock::now() - start;
                if (elapsed >= timeLimit) break;
                if (!frameEndQueue.Flush(false, timeLimit - elapsed)) allReady = false;
            } while (blockUntilReady && !allReady);
        }
    }

    CommandContextPtr DeviceContext::GetFrameCommandContext(render_graph::Resources *resources) {
        Assert(std::this_thread::get_id() == renderThread, "GetFrameCommandContext must be called from render thread");

        auto &fence = Frame().inFlightFence;
        Assertf(device->getFenceStatus(fence->get()) == vk::Result::eNotReady,
            "CommandContext created with invalid frame fence");
        Frame().fenceInUse = true;

        CommandContextPtr cmd;
        if (Frame().nextIndex < Frame().commandContexts.size()) {
            cmd = Frame().commandContexts[Frame().nextIndex++];

            // Reset cmd to default state
            auto buffer = std::move(cmd->RawRef());
            auto acquiredImages = std::move(cmd->acquiredImages);
            auto acquiredBuffers = std::move(cmd->acquiredBuffers);
            auto waitSemaphores = std::move(cmd->waitSemaphores);
            auto signalSemaphores = std::move(cmd->signalSemaphores);
            auto waitStages = std::move(cmd->waitStages);
            acquiredImages.clear();
            acquiredBuffers.clear();
            waitSemaphores.clear();
            signalSemaphores.clear();
            waitStages.clear();
            cmd->~CommandContext();
            new (cmd.get()) CommandContext(*this, std::move(buffer), QueueID::General, CommandContextType::Frame);
            cmd->acquiredImages = std::move(acquiredImages);
            cmd->acquiredBuffers = std::move(acquiredBuffers);
            cmd->waitSemaphores = std::move(waitSemaphores);
            cmd->signalSemaphores = std::move(signalSemaphores);
            cmd->waitStages = std::move(waitStages);
        } else {
            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.commandPool = *Frame().commandPool;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandBufferCount = 1;
            auto buffers = device->allocateCommandBuffersUnique(allocInfo);

            cmd = std::make_shared<CommandContext>(*this,
                std::move(buffers[0]),
                QueueID::General,
                CommandContextType::Frame);
            Frame().commandContexts.push_back(cmd);
            Frame().nextIndex++;
        }
        cmd->handle = cmd;
        cmd->Begin(resources);
        return cmd;
    }

    CommandContextPtr DeviceContext::GetFencedCommandContext(QueueID queueId) {
        auto &thr = Thread();
        queueId = QueueID::General;
        auto cmd = thr.queues[queueId].commandContexts->Get();
        thr.queues[queueId].pendingCommandContexts.push_back(cmd);
        cmd->handle = cmd;
        cmd->Begin();
        return cmd;
    }

    void DeviceContext::SubmitInternal(std::span<const CommandContextPtr> cmds) {
        ZoneScoped;
        Assert(std::this_thread::get_id() == submitThread, "Submit must be called from a single thread");
        Assert(!cmds.empty(), "DeviceContext::SubmitInternal called with empty command list");

        std::optional<QueueID> queueId;
        std::shared_ptr<vk::UniqueFence> fence;
        static thread_local std::vector<vk::Semaphore> signalSemArray;
        static thread_local std::vector<vk::Semaphore> waitSemArray;
        static thread_local std::vector<vk::PipelineStageFlags> waitStageArray;
        for (auto &cmd : cmds) {
            auto cmdFence = cmd->Fence();
            auto cmdQueueId = cmd->GetQueueID();

            if (cmd->WritesToSwapchain()) {
                waitStageArray.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
                waitSemArray.push_back(*Frame().imageAvailableSemaphore);
                signalSemArray.push_back(*SwapchainImage().submitCompleteSemaphore);
            }

            signalSemArray.reserve(signalSemArray.size() + cmd->signalSemaphores.size());
            for (SemaphorePtr &sem : cmd->signalSemaphores) {
                signalSemArray.emplace_back(sem->get());
            }
            waitSemArray.reserve(waitSemArray.size() + cmd->waitSemaphores.size());
            for (SemaphorePtr &sem : cmd->waitSemaphores) {
                waitSemArray.emplace_back(sem->get());
            }
            waitStageArray.insert(waitStageArray.end(), cmd->waitStages.begin(), cmd->waitStages.end());

            if (cmdFence) {
                Assert(!fence || fence == cmdFence, "can't submit with multiple fences");
                fence = cmdFence;
            }

            Assert(!queueId || cmdQueueId == queueId.value(), "can't submit with multiple queues");
            queueId = cmdQueueId;
        }

        static thread_local std::vector<vk::CommandBuffer> cmdBufs;
        cmdBufs.reserve(cmds.size());
        for (auto &cmd : cmds) {
            cmdBufs.emplace_back(cmd->Raw());
        }
        DebugAssert(waitSemArray.size() == waitStageArray.size(),
            "must have exactly one wait stage per wait semaphore");

        vk::SubmitInfo submitInfo = {};
        submitInfo.waitSemaphoreCount = waitSemArray.size();
        submitInfo.pWaitSemaphores = waitSemArray.data();
        submitInfo.pWaitDstStageMask = waitStageArray.data();
        submitInfo.signalSemaphoreCount = signalSemArray.size();
        submitInfo.pSignalSemaphores = signalSemArray.data();
        submitInfo.commandBufferCount = cmdBufs.size();
        submitInfo.pCommandBuffers = cmdBufs.data();

        queues[*queueId].lastSubmit = frameCounter;
        try {
            ZoneScopedN("VkQueueSubmit");
            Assertf(fence, "SubmitInternal null fence");
            if (cmds.front()->type == CommandContextType::Fence) {
                Assertf(device->getFenceStatus(fence->get()) == vk::Result::eSuccess,
                    "VkQueueSubmit previous frame did not complete");
                device->resetFences({fence->get()});
            }
            Assertf(device->getFenceStatus(fence->get()) == vk::Result::eNotReady,
                "VkQueueSubmit previous fence is already used");
            queues[*queueId].queue.submit({submitInfo}, fence->get());
        } catch (const vk::DeviceLostError &err) {
            Errorf("Device lost! queue.submit() %s", err.what());
            deviceResetRequired = true;
        }
        for (auto &cmd : cmds) {
            Assert(!cmd->submitted.exchange(true), "CommandContext submitted twice");
            cmd->submitted.notify_all();
        }

        signalSemArray.clear();
        waitSemArray.clear();
        waitStageArray.clear();
        cmdBufs.clear();
    }

    void DeviceContext::Submit(const DispatchSourceInfo &sourceInfo, const CommandContextPtr &cmd) {
        if (cmd->recording) cmd->End();
        if (std::this_thread::get_id() == submitThread) {
            SubmitInternal({&cmd, 1});
        } else {
            submitQueue.Dispatch<void>(sourceInfo, [this, cmd = cmd] {
                SubmitInternal({&cmd, 1});
            });
        }
    }

    void DeviceContext::Submit(const DispatchSourceInfo &sourceInfo, std::vector<CommandContextPtr> &&cmds) {
        for (auto &cmd : cmds) {
            if (cmd->recording) cmd->End();
        }
        if (std::this_thread::get_id() == submitThread) {
            SubmitInternal(cmds);
            cmds.clear();
        } else {
            submitQueue.Dispatch<void>(sourceInfo, [this, cmds = std::move(cmds)] {
                SubmitInternal(cmds);
            });
        }
    }

    BufferPtr DeviceContext::AllocateBuffer(BufferLayout layout, vk::BufferUsageFlags usage, VmaMemoryUsage residency) {
        DebugAssert(usage != vk::BufferUsageFlags(), "AllocateBuffer called without usage flags");
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = layout.size;
        bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        return Buffer::New(bufferInfo, allocInfo, allocator.get(), layout.arrayStride, layout.arrayCount);
    }

    BufferPtr DeviceContext::AllocateBuffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo) {
        return Buffer::New(bufferInfo, allocInfo, allocator.get());
    }

    BufferPtr DeviceContext::GetBuffer(const BufferDesc &desc) {
        return Thread().bufferPool->Get(desc);
    }

    AsyncPtr<Buffer> DeviceContext::CreateUploadBuffer(const DispatchSourceInfo &sourceInfo,
        const InitialData &data,
        vk::BufferUsageFlags usage) {
        if (!data.data) return nullptr;
        return allocatorQueue.Dispatch<Buffer>(sourceInfo, [data, usage, this] {
            vk::BufferCreateInfo bufferInfo = {};
            bufferInfo.size = data.dataSize;
            bufferInfo.usage = usage;
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

            Assertf(data.data, "DeviceContext::CreateBuffer null initial buffer data");
            auto buf = AllocateBuffer(bufferInfo, allocInfo);
            buf->CopyFrom(data.data, data.dataSize);
            buf->SetAccess(Access::None, Access::HostWrite);
            return buf;
        });
    }

    AsyncPtr<Buffer> DeviceContext::CreateBuffer(const DispatchSourceInfo &sourceInfo,
        const InitialData &data,
        vk::BufferCreateInfo bufferInfo,
        VmaAllocationCreateInfo allocInfo) {
        return allocatorQueue.Dispatch<Buffer>(sourceInfo, [=, this]() {
            auto buf = AllocateBuffer(bufferInfo, allocInfo);
            buf->CopyFrom(data.data, data.dataSize);
            buf->SetAccess(Access::None, Access::HostWrite);
            return buf;
        });
    }

    AsyncPtr<Buffer> DeviceContext::CreateBuffer(const DispatchSourceInfo &sourceInfo,
        const InitialData &data,
        vk::BufferUsageFlags usage,
        VmaMemoryUsage residency) {
        return allocatorQueue.Dispatch<Buffer>(sourceInfo, [=, this]() {
            auto buf = AllocateBuffer(data.dataSize, usage, residency);
            buf->CopyFrom(data.data, data.dataSize);
            buf->SetAccess(Access::None, Access::HostWrite);
            return buf;
        });
    }

    ImagePtr DeviceContext::AllocateImage(vk::ImageCreateInfo info,
        VmaMemoryUsage residency,
        vk::ImageUsageFlags declaredUsage) {
        ZoneScoped;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        if (!declaredUsage) declaredUsage = info.usage;
        return Image::New(info, allocInfo, allocator.get(), declaredUsage);
    }

    AsyncPtr<Image> DeviceContext::CreateImage(const DispatchSourceInfo &sourceInfo,
        ImageCreateInfo createInfo,
        const InitialData &initialData) {
        if (!initialData.data) {
            Assert(!createInfo.genMipmap, "DeviceContext::CreateImage must pass initial data to generate a mipmap");
            return CreateImage(sourceInfo, createInfo, AsyncPtr<Buffer>());
        } else {
            return CreateImage(sourceInfo, createInfo, CreateUploadBuffer(sourceInfo, initialData));
        }
    }

    AsyncPtr<Image> DeviceContext::CreateImage(const DispatchSourceInfo &sourceInfo,
        ImageCreateInfo createInfo,
        const AsyncPtr<Buffer> &uploadBuffer) {
        ZoneScoped;

        bool genFactor = !createInfo.factor.empty();
        vk::ImageUsageFlags declaredUsage = createInfo.usage;
        vk::Format factorFormat = createInfo.format;

        if (!createInfo.mipLevels) {
            createInfo.mipLevels = createInfo.genMipmap ? CalculateMipmapLevels(createInfo.extent) : 1;
        }
        if (!createInfo.arrayLayers) createInfo.arrayLayers = 1;
        bool genMipmap = createInfo.genMipmap && createInfo.mipLevels > 1;

        if (!uploadBuffer) {
            Assert(!genMipmap, "DeviceContext::CreateImage must pass upload buffer to generate a mipmap");
        } else {
            Assert(createInfo.arrayLayers == 1, "can't load initial data into an image array");
            Assert(!genMipmap || createInfo.mipLevels > 1, "can't generate mipmap for a single level image");

            createInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
            if (genMipmap) createInfo.usage |= vk::ImageUsageFlagBits::eTransferSrc;
            if (genFactor) {
                createInfo.flags |= vk::ImageCreateFlagBits::eExtendedUsage;
                createInfo.usage |= vk::ImageUsageFlagBits::eStorage;
                if (FormatIsSRGB(createInfo.format)) {
                    factorFormat = FormatSRGBToUnorm(createInfo.format);
                    createInfo.flags |= vk::ImageCreateFlagBits::eMutableFormat;
                    createInfo.formats.push_back(createInfo.format);
                    createInfo.formats.push_back(factorFormat);
                }
            }
        }

        auto futImage = allocatorQueue.Dispatch<Image>(sourceInfo, [this, createInfo, declaredUsage]() {
            auto actualCreateInfo = createInfo.GetVkCreateInfo();
            auto formatInfo = createInfo.GetVkFormatList();
            if (formatInfo.viewFormatCount > 0) actualCreateInfo.pNext = &formatInfo;

            return AllocateImage(actualCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, declaredUsage);
        });

        if (uploadBuffer) {
            futImage = UpdateImage(sourceInfo, futImage, uploadBuffer, false);

            if (genFactor) {
                futImage = allocatorQueue.Dispatch<Image>(sourceInfo,
                    futImage,
                    [this, factorFormat, createInfo](ImagePtr image) {
                        if (!image) return std::shared_ptr<Image>();

                        ZoneScopedN("ApplyFactor");
                        auto factorCmd = GetFencedCommandContext(QueueID::ComputeAsync);
                        factorCmd->AcquireImage(image, Access::ComputeShaderReadWrite);

                        ImageViewPtr factorView;
                        {
                            GPUZone(this, factorCmd, "ApplyFactor");

                            ImageViewCreateInfo factorViewInfo = {};
                            factorViewInfo.image = image;
                            factorViewInfo.format = factorFormat;
                            factorViewInfo.mipLevelCount = 1;
                            factorViewInfo.usage = vk::ImageUsageFlagBits::eStorage;
                            factorView = CreateImageView(factorViewInfo);

                            factorCmd->SetComputeShader("texture_factor.comp");
                            factorCmd->SetImageView("texture", factorView);

                            struct {
                                glm::vec4 factor;
                                int components;
                                bool srgb;
                            } factorPushConstants;

                            for (size_t i = 0; i < createInfo.factor.size(); i++) {
                                factorPushConstants.factor[i] = (float)createInfo.factor[i];
                            }
                            factorPushConstants.components = createInfo.factor.size();
                            factorPushConstants.srgb = FormatIsSRGB(createInfo.format);
                            factorCmd->PushConstants(factorPushConstants);

                            factorCmd->Dispatch((createInfo.extent.width + 15) / 16,
                                (createInfo.extent.height + 15) / 16,
                                1);
                        }
                        Submit(NewDispatchSource, factorCmd);
                        return image;
                    });
            }
            if (genMipmap) futImage = UpdateImageMipmap(sourceInfo, futImage);
        }
        return futImage;
    }

    AsyncPtr<Image> DeviceContext::UpdateImage(const DispatchSourceInfo &sourceInfo,
        const AsyncPtr<Image> &dstImage,
        const AsyncPtr<Buffer> &srcBuffer,
        bool updateMipmap) {
        auto futImage = allocatorQueue.Dispatch<Image>(sourceInfo,
            dstImage,
            srcBuffer,
            [this](ImagePtr image, BufferPtr stagingBuf) {
                if (!image) return std::shared_ptr<Image>();

                ZoneScopedN("PrepareImage");
                auto transferCmd = GetFencedCommandContext(QueueID::TransferAsync);
                transferCmd->AcquireBuffer(stagingBuf, Access::TransferRead);
                transferCmd->AcquireImage(image, Access::TransferWrite);

                vk::BufferImageCopy region;
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = FormatToAspectFlags(image->Format());
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = vk::Offset3D{0, 0, 0};
                region.imageExtent = image->Extent();

                transferCmd->Raw().copyBufferToImage(*stagingBuf,
                    *image,
                    vk::ImageLayout::eTransferDstOptimal,
                    {region});

                {
                    ZoneScopedN("CopyBufferToImage");
                    Submit(NewDispatchSource, transferCmd);
                }

                return image;
            });
        if (updateMipmap) futImage = UpdateImageMipmap(sourceInfo, futImage);
        return futImage;
    }

    AsyncPtr<Image> DeviceContext::UpdateImageMipmap(const DispatchSourceInfo &sourceInfo,
        const AsyncPtr<Image> &image) {
        ZoneScoped;
        return allocatorQueue.Dispatch<Image>(sourceInfo, image, [this](ImagePtr image) {
            if (!image) return std::shared_ptr<Image>();
            // TODO: Add GraphicsAsync
            auto graphicsCmd = GetFencedCommandContext(QueueID::General);
            graphicsCmd->AcquireImage(image, Access::TransferReadWrite);

            {
                GPUZone(this, graphicsCmd, "Mipmap");

                ImageBarrierInfo transferMips = {};
                transferMips.trackImageLayout = false;
                transferMips.baseMipLevel = 1;
                transferMips.mipLevelCount = image->MipLevels() - 1;

                graphicsCmd->ImageBarrier(image,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    {},
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    transferMips);

                vk::Offset3D currentExtent = {(int32_t)image->Extent().width,
                    (int32_t)image->Extent().height,
                    (int32_t)image->Extent().depth};
                transferMips.mipLevelCount = 1;

                for (uint32_t i = 1; i < image->MipLevels(); i++) {
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
                        vk::AccessFlagBits::eTransferWrite,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::AccessFlagBits::eTransferRead,
                        transferMips);
                }

                // Each mip has now been transitioned to TransferSrc.
                image->SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
            }
            Submit(NewDispatchSource, graphicsCmd);
            return image;
        });
    }

    ImageViewPtr DeviceContext::CreateImageView(ImageViewCreateInfo info) {
        if (info.format == vk::Format::eUndefined) info.format = info.image->Format();

        if (info.arrayLayerCount == VK_REMAINING_ARRAY_LAYERS) {
            info.arrayLayerCount = info.image->ArrayLayers() - info.baseArrayLayer;
        }

        if (info.mipLevelCount == VK_REMAINING_MIP_LEVELS) {
            info.mipLevelCount = info.image->MipLevels() - info.baseMipLevel;
        }

        vk::ImageViewCreateInfo createInfo = {};
        createInfo.image = *info.image;
        createInfo.format = info.format;
        createInfo.viewType = info.viewType;
        createInfo.components = info.mapping;
        createInfo.subresourceRange.aspectMask = info.aspectMask ? info.aspectMask : FormatToAspectFlags(info.format);
        createInfo.subresourceRange.baseMipLevel = info.baseMipLevel;
        createInfo.subresourceRange.levelCount = info.mipLevelCount;
        createInfo.subresourceRange.baseArrayLayer = info.baseArrayLayer;
        createInfo.subresourceRange.layerCount = info.arrayLayerCount;

        // By default, pick the same usage passed in sp::vulkan::ImageCreateInfo.
        if (!info.usage) info.usage = info.image->DeclaredUsage();

        // The actual underlying image usage may have extra flags.
        auto imageFullUsage = info.image->Usage();

        vk::ImageViewUsageCreateInfo usageCreateInfo;
        if (info.usage != imageFullUsage) {
            Assert((info.usage & imageFullUsage) == info.usage, "view usage must be a subset of the image usage");
            usageCreateInfo.usage = info.usage;
            createInfo.pNext = &usageCreateInfo;
        }

        return std::make_shared<ImageView>(device->createImageViewUnique(createInfo), info);
    }

    AsyncPtr<ImageView> DeviceContext::CreateImageAndView(const DispatchSourceInfo &sourceInfo,
        const ImageCreateInfo &imageInfo,
        const ImageViewCreateInfo &viewInfo,
        const InitialData &data) {
        ZoneScoped;
        auto futImage = CreateImage(sourceInfo, imageInfo, data);

        auto futView = allocatorQueue.Dispatch<ImageView>(sourceInfo, futImage, [=, this](ImagePtr image) {
            auto viewI = viewInfo;
            viewI.image = image;
            return CreateImageView(viewI);
        });
        return allocatorQueue.Dispatch<ImageView>(sourceInfo, futView, [this](ImageViewPtr imageView) {
            auto cmd = GetFencedCommandContext(QueueID::General);
            auto &image = imageView->Image();
            if (image->Usage() & vk::ImageUsageFlagBits::eSampled) {
                cmd->ImageBarrier(image,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
                    vk::AccessFlagBits::eShaderRead);
                image->SetAccess(Access::None, Access::FragmentShaderSampleImage);
            } else if (image->Usage() & vk::ImageUsageFlagBits::eStorage) {
                cmd->ImageBarrier(image,
                    vk::ImageLayout::eGeneral,
                    vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
                    vk::AccessFlagBits::eShaderRead);
                image->SetAccess(Access::None, Access::FragmentShaderReadStorage);
            }
            Submit(NewDispatchSource, cmd);
            return imageView;
        });
    }

    AsyncPtr<ImageView> DeviceContext::LoadAssetImage(std::string_view path, bool genMipmap, bool srgb) {
        auto futImage = Assets().LoadImage(path);
        return allocatorQueue.Dispatch<ImageView>(NewDispatchSource,
            futImage,
            [this, path = AssetPath(path), genMipmap, srgb](std::shared_ptr<sp::Image> image) {
                if (!image) {
                    Warnf("Missing asset image: %s", path);
                    return CreateSinglePixel(ERROR_COLOR);
                }
                return LoadImage(image, genMipmap, srgb);
            });
    }

    AsyncPtr<ImageView> DeviceContext::LoadImage(std::shared_ptr<const sp::Image> image, bool genMipmap, bool srgb) {
        ZoneScoped;
        Assertf(image, "DeviceContext::LoadImage called with null image");
        ImageCreateInfo createInfo = {};
        createInfo.extent = vk::Extent3D(image->GetWidth(), image->GetHeight(), 1);
        Assert(createInfo.extent.width > 0 && createInfo.extent.height > 0, "image has zero size");

        createInfo.format = FormatFromTraits(image->GetComponents(), 8, srgb);
        Assert(createInfo.format != vk::Format::eUndefined, "invalid image format");

        createInfo.genMipmap = genMipmap && createInfo.extent.width > 1 && createInfo.extent.height > 1;
        createInfo.usage = vk::ImageUsageFlagBits::eSampled;

        const uint8_t *data = image->GetImage().get();
        Assert(data, "missing image data");

        ImageViewCreateInfo viewInfo = {};
        if (genMipmap) {
            viewInfo.defaultSampler = GetSampler(SamplerType::TrilinearTiled);
        } else {
            viewInfo.defaultSampler = GetSampler(SamplerType::BilinearClampEdge);
        }
        return CreateImageAndView(NewDispatchSource, createInfo, viewInfo, {data, image->ByteSize(), image});
    }

    AsyncPtr<ImageView> DeviceContext::CreateSinglePixel(color_alpha_t color) {
        glm::u8vec4 value(color);

        ImageCreateInfo imageInfo = {};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageInfo.extent = vk::Extent3D(1, 1, 1);

        ImageViewCreateInfo viewInfo = {};
        viewInfo.defaultSampler = GetSampler(SamplerType::NearestTiled);
        return CreateImageAndView(NewDispatchSource, imageInfo, viewInfo, {&value[0], sizeof(value)});
    }

    vk::Sampler DeviceContext::GetSampler(SamplerType type) {
        auto &sampler = namedSamplers[type];
        if (sampler) return *sampler;

        vk::SamplerCreateInfo samplerInfo;

        switch (type) {
        case SamplerType::BilinearClampBorder:
        case SamplerType::BilinearClampEdge:
        case SamplerType::BilinearTiled:
        case SamplerType::TrilinearClampBorder:
        case SamplerType::TrilinearClampEdge:
        case SamplerType::TrilinearTiled:
            samplerInfo.magFilter = vk::Filter::eLinear;
            samplerInfo.minFilter = vk::Filter::eLinear;
            break;
        case SamplerType::NearestClampBorder:
        case SamplerType::NearestClampEdge:
        case SamplerType::NearestTiled:
            samplerInfo.magFilter = vk::Filter::eNearest;
            samplerInfo.minFilter = vk::Filter::eNearest;
            break;
        }

        switch (type) {
        case SamplerType::TrilinearClampBorder:
        case SamplerType::TrilinearClampEdge:
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
        case SamplerType::TrilinearClampEdge:
        case SamplerType::BilinearClampEdge:
        case SamplerType::NearestClampEdge:
            samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
            break;
        default:
            samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
            samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
            samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
        }

        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        sampler = device->createSamplerUnique(samplerInfo);
        return *sampler;
    }

    vk::Sampler DeviceContext::GetSampler(const vk::SamplerCreateInfo &info) {
        Assert(info.pNext == 0, "sampler info pNext can't be set");

        SamplerKey key((const VkSamplerCreateInfo &)info);
        auto &sampler = adhocSamplers[key];
        if (sampler) return *sampler;

        sampler = device->createSamplerUnique(info);
        return *sampler;
    }

    ShaderHandle DeviceContext::LoadShader(std::string_view name) {
        auto it = shaderHandles.find(name);
        if (it != shaderHandles.end()) return it->second;

        std::string strName(name);
        auto shader = shaders.emplace_back(CreateShader(strName, {}));
        auto handle = static_cast<ShaderHandle>(shaders.size());
        shaderHandles[strName] = handle;
        return handle;
    }

    std::shared_ptr<Shader> DeviceContext::CreateShader(const std::string &name, Hash64 compareHash) {
        ZoneScoped;
        ZoneStr(name);
        auto asset = Assets().Load("shaders/" + name + ".spv", AssetType::Bundled, compareHash != Hash64())->Get();
        Assertf(asset, "could not load shader: %s", name);

        auto newHash = Hash128To64(asset->Hash());
        if (compareHash == newHash) return nullptr;

        vk::ShaderModuleCreateInfo shaderCreateInfo;
        shaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(asset->BufferPtr());
        shaderCreateInfo.codeSize = asset->BufferSize();

        auto shaderModule = device->createShaderModuleUnique(shaderCreateInfo);

        auto reflection = spv_reflect::ShaderModule(asset->BufferSize(), asset->BufferPtr());
        if (reflection.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
            Abortf("could not parse shader: %s error: %d", name, reflection.GetResult());
        }

        return std::make_shared<Shader>(name, std::move(shaderModule), std::move(reflection), newHash);
    }

    std::shared_ptr<Shader> DeviceContext::GetShader(ShaderHandle handle) const {
        if (handle == 0 || shaders.size() < (size_t)handle) return nullptr;

        return shaders[handle - 1];
    }

    std::shared_ptr<Pipeline> DeviceContext::GetPipeline(const PipelineCompileInput &input) {
        return pipelinePool->GetPipeline(input);
    }

    ImageViewPtr DeviceContext::SwapchainImageView() {
        return swapchain ? SwapchainImage().imageView : nullptr;
    }

    std::shared_ptr<RenderPass> DeviceContext::GetRenderPass(const RenderPassInfo &info) {
        return renderPassPool->GetRenderPass(info);
    }

    std::shared_ptr<Framebuffer> DeviceContext::GetFramebuffer(const RenderPassInfo &info) {
        return framebufferPool->GetFramebuffer(info);
    }

    vk::DescriptorSet DeviceContext::CreateBindlessDescriptorSet() {
        if (!bindlessImageSamplerDescriptorPool) {
            DescriptorSetLayoutInfo layout;
            layout.sampledImagesMask = 1; // first binding is a sampled image array
            layout.descriptorCount[0] = 0; // of unbounded array size
            layout.stages[0] = vk::ShaderStageFlagBits::eAll;
            bindlessImageSamplerDescriptorPool = pipelinePool->GetDescriptorPool(layout);
        }

        return bindlessImageSamplerDescriptorPool->CreateBindlessDescriptorSet();
    }

    DeviceContext::ThreadContext &DeviceContext::Thread() {
        thread_local size_t threadIndex = nextThreadIndex.fetch_add(1);
        thread_local bool firstInit = true;
        Assert(threadIndex < threadContexts.size(), "ran out of thread contexts");
        auto &threadContext = *threadContexts[threadIndex];
        if (firstInit) {
            firstInit = false; // Set first because Reserve() recursively calls Thread()
            for (const QueueID &queueId : magic_enum::enum_values<QueueID>()) {
                threadContext.queues[queueId].commandContexts->Reserve(64);
            }
        }
        return threadContext;
    }

    void DeviceContext::ThreadContext::ReleaseAvailableResources() {
        ZoneScoped;
        size_t erased = 1;
        while (erased > 0) {
            erased = 0;
            for (const QueueID &queueId : magic_enum::enum_values<QueueID>()) {
                erased += erase_if(queues[queueId].pendingCommandContexts, [](CommandContextPtr &cmd) {
                    if (!cmd->submitted) return false;
                    auto &fence = cmd->Fence();
                    Assertf(fence, "Pending CommandContext is missing a fence");
                    if (fence.use_count() == 1 && cmd->Device()->getFenceStatus(fence->get()) == vk::Result::eSuccess) {
                        return true;
                    }
                    return false;
                });
            }
        }

        bufferPool->Tick();
        if (printBufferStats.exchange(false)) bufferPool->LogStats();
    }

    DeviceContext::FrameContext &DeviceContext::Frame() {
        return frameContexts[frameIndex];
    }

    vk::FormatProperties DeviceContext::FormatProperties(vk::Format format) const {
        return selectedDevice.getFormatProperties(format);
    }

    vk::Format DeviceContext::SelectSupportedFormat(vk::FormatProperties requiredProps,
        vk::ArrayProxy<const vk::Format> possibleFormats) {

        auto reqOptimal = requiredProps.optimalTilingFeatures, reqLinear = requiredProps.linearTilingFeatures,
             reqBuffer = requiredProps.bufferFeatures;

        for (auto format : possibleFormats) {
            auto props = FormatProperties(format);
            if (reqOptimal && (props.optimalTilingFeatures & reqOptimal) != reqOptimal) continue;
            if (reqLinear && (props.linearTilingFeatures & reqLinear) != reqLinear) continue;
            if (reqBuffer && (props.bufferFeatures & reqBuffer) != reqBuffer) continue;
            return format;
        }

        std::stringstream err;
        err << "device does not support any format from list:";
        for (auto format : possibleFormats) {
            err << " " << vk::to_string(format);
        }
        if (reqOptimal) err << ", having optimal tiling features: " << vk::to_string(reqOptimal);
        if (reqLinear) err << ", having linear tiling features: " << vk::to_string(reqLinear);
        if (reqBuffer) err << ", having buffer features: " << vk::to_string(reqBuffer);
        Abortf("%s", err.str());
    }

#ifdef TRACY_ENABLE_GRAPHICS
    tracy::VkCtx *DeviceContext::GetTracyContext(QueueID queueId) {
        return tracing.tracyContexts[queueId];
    }
#endif

    void *DeviceContext::Win32WindowHandle() {
        return graphics.windowHandlers.win32_handle;
    }
} // namespace sp::vulkan
