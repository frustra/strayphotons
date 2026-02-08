/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "DeviceContext.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Image.hh"
#include "common/InlineVector.hh"
#include "common/Logging.hh"
#include "console/CFunc.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/components/GuiElement.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/core/GraphicsManager.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/vulkan/Compositor.hh"
#include "graphics/vulkan/ProfilerGui.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"
#include "graphics/vulkan/core/Pipeline.hh"
#include "graphics/vulkan/core/RenderPass.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/core/VkTracing.hh"

#include <algorithm>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>

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
        string message(pCallbackData->pMessage);

        switch (messageSeverity) {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
    #ifdef TRACY_ENABLE_GRAPHICS
            // Ignore Tracy timer query errors
            if (message.find("CoreValidation-DrawState-QueryNotReset") != string_view::npos) break;
    #endif
            if (message.find("(subresource: aspectMask 0x1 array layer 0, mip level 0) to be in layout "
                             "VK_IMAGE_LAYOUT_GENERAL--instead, current layout is VK_IMAGE_LAYOUT_PREINITIALIZED.") !=
                string_view::npos)
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
        string message(pCallbackData->pMessage);

        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    #ifdef TRACY_ENABLE_GRAPHICS
            if (message.find("CoreValidation-DrawState-QueryNotReset") != string_view::npos) {
                // Ignore Tracy timer query errors
                return VK_FALSE;
            }
    #endif
            if (message.find("(subresource: aspectMask 0x1 array layer 0, mip level 0) to be in layout "
                             "VK_IMAGE_LAYOUT_GENERAL--instead, current layout is VK_IMAGE_LAYOUT_PREINITIALIZED.") !=
                string_view::npos)
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

    DeviceContext::DeviceContext(GraphicsManager &graphics, bool enableValidationLayers)
        : graphics(graphics), mainThread(std::this_thread::get_id()), allocator(nullptr, nullptr), threadContexts(32),
          frameBeginQueue("BeginFrame", 0, {}), frameEndQueue("EndFrame", 0, {}), allocatorQueue("GPUAllocator", 1, {}),
          graph(*this) {
        ZoneScoped;

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

            std::vector<const char *> layers;
            if (enableValidationLayers) {
                layers.emplace_back("VK_LAYER_KHRONOS_validation");
            }

            bool hasMemoryRequirements2Ext = false, hasDedicatedAllocationExt = false;
            auto availableExtensions = vk::enumerateInstanceExtensionProperties();
            // Debugf("Available Vulkan extensions: %u", availableExtensions.size());
            for (auto &ext : availableExtensions) {
                string_view name(ext.extensionName.data());
                // Debugf("\t%s", name);

                if (name == VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) {
                    hasMemoryRequirements2Ext = true;
                } else if (name == VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) {
                    hasDedicatedAllocationExt = true;
                }
            }

            auto physicalDevices = instance.enumeratePhysicalDevices();
            // TODO: Prioritize discrete GPUs and check for capabilities like Geometry/Compute shaders
            if (physicalDevices.size() > 0) {
                // TODO: Check device extension support
                physicalDeviceProperties.pNext = &physicalDeviceDescriptorIndexingProperties;
                physicalDevices.front().getProperties2(&physicalDeviceProperties);
                // auto features = device.getFeatures();
                Logf("Using graphics device: %s", physicalDeviceProperties.properties.deviceName.data());
                physicalDevice = physicalDevices.front();
            }
            Assert(physicalDevice, "No suitable graphics device found!");

            std::array<uint32_t, QUEUE_TYPES_COUNT> queueIndex;
            auto queueFamilies = physicalDevice.getQueueFamilyProperties();
            vector<uint32_t> queuesUsedCount(queueFamilies.size());
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
                    if (surfaceSupport && !physicalDevice.getSurfaceSupportKHR(i, surface)) continue;
                    if (queuesUsedCount[i] >= props.queueCount) continue;

                    queueFamilyIndex[queueType] = i;
                    queueIndex[queueType] = queuesUsedCount[i]++;
                    queuePriority[i].push_back(priority);
                    return true;
                }
                return false;
            };

            if (!findQueue(QUEUE_TYPE_GRAPHICS,
                    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute,
                    {},
                    1.0f,
                    enableSwapchain))
                Abortf("could not find a supported graphics queue family");

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

            // currently we have code that assumes the transfer queue family is different from the other queues
            // Assert(queueFamilyIndex[QUEUE_TYPE_TRANSFER] != queueFamilyIndex[QUEUE_TYPE_GRAPHICS],
            //    "transfer queue family overlaps graphics queue");

            std::vector<vk::DeviceQueueCreateInfo> queueInfos;
            for (uint32_t i = 0; i < queueFamilies.size(); i++) {
                if (queuesUsedCount[i] == 0) continue;

                vk::DeviceQueueCreateInfo queueInfo;
                queueInfo.queueFamilyIndex = i;
                queueInfo.queueCount = queuesUsedCount[i];
                queueInfo.pQueuePriorities = queuePriority[i].data();
                queueInfos.push_back(queueInfo);
            }

            vector<const char *> enabledDeviceExtensions = {VK_KHR_MULTIVIEW_EXTENSION_NAME,
                // VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
                VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
                VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
                VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
                "VK_KHR_portability_subset"};

            if (enableSwapchain) {
                enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }

            auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();

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

            vk::PhysicalDeviceVulkan12Features availableVulkan12Features;
            vk::PhysicalDeviceVulkan11Features availableVulkan11Features;
            availableVulkan11Features.pNext = &availableVulkan12Features;
            vk::PhysicalDeviceFeatures2 deviceFeatures2;
            deviceFeatures2.pNext = &availableVulkan11Features;

            physicalDevice.getFeatures2KHR(&deviceFeatures2);

            const auto &availableDeviceFeatures = deviceFeatures2.features;
            // Assert(availableDeviceFeatures.fillModeNonSolid, "device must support fillModeNonSolid");
            Assert(availableDeviceFeatures.samplerAnisotropy, "device must support samplerAnisotropy");
            Assert(availableDeviceFeatures.multiDrawIndirect, "device must support multiDrawIndirect");
            Assert(availableDeviceFeatures.multiViewport, "device must support multiViewport");
            Assert(availableDeviceFeatures.drawIndirectFirstInstance, "device must support drawIndirectFirstInstance");
            Assert(availableDeviceFeatures.shaderInt16, "device must support shaderInt16");
            Assert(availableDeviceFeatures.fragmentStoresAndAtomics, "device must support fragmentStoresAndAtomics");
            // Assert(availableDeviceFeatures.wideLines, "device must support wideLines");
            Assert(availableVulkan11Features.multiview, "device must support multiview");
            Assert(availableVulkan11Features.shaderDrawParameters, "device must support shaderDrawParameters");
            Assert(availableVulkan11Features.storageBuffer16BitAccess, "device must support storageBuffer16BitAccess");
            Assert(availableVulkan11Features.uniformAndStorageBuffer16BitAccess,
                "device must support uniformAndStorageBuffer16BitAccess");
            Assert(availableVulkan12Features.shaderOutputViewportIndex,
                "device must support shaderOutputViewportIndex");
            Assert(availableVulkan12Features.shaderOutputLayer, "device must support shaderOutputLayer");
            // Assert(availableVulkan12Features.drawIndirectCount, "device must support drawIndirectCount");
            Assert(availableVulkan12Features.runtimeDescriptorArray, "device must support runtimeDescriptorArray");
            Assert(availableVulkan12Features.descriptorBindingPartiallyBound,
                "device must support descriptorBindingPartiallyBound");
            Assert(availableVulkan12Features.descriptorBindingVariableDescriptorCount,
                "device must support descriptorBindingVariableDescriptorCount");
            Assert(availableVulkan12Features.shaderSampledImageArrayNonUniformIndexing,
                "device must support shaderSampledImageArrayNonUniformIndexing");
            Assert(availableVulkan12Features.descriptorBindingUpdateUnusedWhilePending,
                "device must support descriptorBindingUpdateUnusedWhilePending");

            vk::PhysicalDeviceVulkan12Features enabledVulkan12Features;
            enabledVulkan12Features.shaderOutputViewportIndex = true;
            enabledVulkan12Features.shaderOutputLayer = true;
            enabledVulkan12Features.drawIndirectCount = false;
            enabledVulkan12Features.runtimeDescriptorArray = true;
            enabledVulkan12Features.descriptorBindingPartiallyBound = true;
            enabledVulkan12Features.descriptorBindingVariableDescriptorCount = true;
            enabledVulkan12Features.shaderSampledImageArrayNonUniformIndexing = true;
            enabledVulkan12Features.descriptorBindingUpdateUnusedWhilePending = true;

            vk::PhysicalDeviceVulkan11Features enabledVulkan11Features;
            enabledVulkan11Features.storageBuffer16BitAccess = true;
            enabledVulkan11Features.uniformAndStorageBuffer16BitAccess = true;
            enabledVulkan11Features.multiview = true;
            enabledVulkan11Features.shaderDrawParameters = true;
            enabledVulkan11Features.pNext = &enabledVulkan12Features;

            vk::PhysicalDeviceFeatures2 enabledDeviceFeatures2;
            enabledDeviceFeatures2.pNext = &enabledVulkan11Features;
            auto &enabledDeviceFeatures = enabledDeviceFeatures2.features;
            enabledDeviceFeatures.dualSrcBlend = true;
            enabledDeviceFeatures.fillModeNonSolid = false;
            enabledDeviceFeatures.samplerAnisotropy = true;
            enabledDeviceFeatures.multiDrawIndirect = true;
            enabledDeviceFeatures.drawIndirectFirstInstance = true;
            enabledDeviceFeatures.multiViewport = true;
            enabledDeviceFeatures.shaderInt16 = true;
            enabledDeviceFeatures.fragmentStoresAndAtomics = true;
            enabledDeviceFeatures.wideLines = false;

            vk::DeviceCreateInfo deviceInfo;
            deviceInfo.queueCreateInfoCount = queueInfos.size();
            deviceInfo.pQueueCreateInfos = queueInfos.data();
            deviceInfo.pNext = &enabledDeviceFeatures2;
            deviceInfo.enabledExtensionCount = enabledDeviceExtensions.size();
            deviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            deviceInfo.enabledLayerCount = layers.size();
            deviceInfo.ppEnabledLayerNames = layers.data();
#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

            device = physicalDevice.createDeviceUnique(deviceInfo, nullptr);
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

#ifdef TRACY_ENABLE_GRAPHICS
            tracing.tracyContexts.resize(QUEUE_TYPES_COUNT);
#endif

            for (uint32_t queueType = 0; queueType < QUEUE_TYPES_COUNT; queueType++) {
                auto familyIndex = queueFamilyIndex[queueType];
                auto queue = device->getQueue(familyIndex, queueIndex[queueType]);
                queues[queueType] = queue;
                queueLastSubmit[queueType] = 0;

#ifdef TRACY_ENABLE_GRAPHICS
                if (queueType != QUEUE_TYPE_COMPUTE && queueType != QUEUE_TYPE_GRAPHICS) continue;

                vk::CommandPoolCreateInfo cmdPoolInfo;
                cmdPoolInfo.queueFamilyIndex = familyIndex;
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

                tracing.tracyContexts[queueType] = tracy::CreateVkContext(physicalDevice,
                    *device,
                    queue,
                    *tracing.cmdBuffers.back(),
                    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
                    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetCalibratedTimestampsEXT);
#endif
            }

            vk::SemaphoreCreateInfo semaphoreInfo = {};
            vk::FenceCreateInfo fenceInfo = {};
            fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

            for (auto &frame : frameContexts) {
                frame.imageAvailableSemaphore = device->createSemaphoreUnique(semaphoreInfo);
                frame.renderCompleteSemaphore = device->createSemaphoreUnique(semaphoreInfo);
                frame.inFlightFence = device->createFenceUnique(fenceInfo);

                for (uint32_t queueType = 0; queueType < QUEUE_TYPES_COUNT; queueType++) {
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
            allocatorInfo.instance = instance;
            allocatorInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT;
            allocatorInfo.preferredLargeHeapBlockSize = 1024ull * 1024 * 1024;
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

            if (hasMemoryRequirements2Ext && hasDedicatedAllocationExt) {
                allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
            }

            VmaAllocator alloc;
            Assert(vmaCreateAllocator(&allocatorInfo, &alloc) == VK_SUCCESS, "allocator init failed");
            allocator = unique_ptr<VmaAllocator_T, void (*)(VmaAllocator)>(alloc, [](VmaAllocator alloc) {
                if (alloc) vmaDestroyAllocator(alloc);
            });

            semaphorePool = make_unique<HandlePool<vk::Semaphore>>(
                [&]() {
                    return device->createSemaphore({});
                },
                [&](vk::Semaphore sem) {
                    device->destroySemaphore(sem);
                });

            fencePool = make_unique<HandlePool<vk::Fence>>(
                [&]() {
                    return device->createFence({});
                },
                [&](vk::Fence fence) {
                    device->destroyFence(fence);
                },
                [&](vk::Fence fence) {
                    device->resetFences({fence});
                });

            pipelinePool = make_unique<PipelineManager>(*this);
            renderPassPool = make_unique<RenderPassManager>(*this);
            framebufferPool = make_unique<FramebufferManager>(*this);

            for (auto &threadContextUnique : threadContexts) {
                threadContextUnique = make_unique<ThreadContext>();
                ThreadContext *threadContext = threadContextUnique.get();

                threadContext->bufferPool = make_unique<BufferPool>(*this);

                for (uint32_t queueType = 0; queueType < QUEUE_TYPES_COUNT; queueType++) {
                    vk::CommandPoolCreateInfo poolInfo;
                    poolInfo.queueFamilyIndex = queueFamilyIndex[queueType];
                    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient |
                                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
                    threadContext->commandPools[queueType] = device->createCommandPoolUnique(poolInfo);

                    threadContext->commandContexts[queueType] = make_unique<HandlePool<CommandContextPtr>>(
                        [this, threadContext, queueType]() {
                            vk::CommandBufferAllocateInfo allocInfo;
                            allocInfo.commandPool = *threadContext->commandPools[queueType];
                            allocInfo.level = vk::CommandBufferLevel::ePrimary;
                            allocInfo.commandBufferCount = 1;
                            auto buffers = device->allocateCommandBuffersUnique(allocInfo);

                            return make_shared<CommandContext>(*this,
                                std::move(buffers[0]),
                                CommandContextType(queueType),
                                CommandContextScope::Fence);
                        },
                        [](CommandContextPtr) {
                            // destroy happens via ~CommandContext
                        },
                        [this](CommandContextPtr cmd) {
                            auto type = cmd->GetType();
                            auto buffer = std::move(cmd->RawRef());
                            auto fence = std::move(cmd->fence);

                            cmd->~CommandContext();
                            new (cmd.get()) CommandContext(*this, std::move(buffer), type, CommandContextScope::Fence);
                            cmd->fence = std::move(fence);

                            cmd->Raw().reset();
                            if (cmd->fence) device->resetFences({cmd->Fence()});
                        });
                }
            }

            funcs = make_unique<CFuncCollection>();
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
    }

    DeviceContext::~DeviceContext() {
        if (vkRenderer) vkRenderer.reset();
        if (device && !deviceResetRequired) device->waitIdle();
        Debugf("Destroying DeviceContext");
        swapchain.reset();
        graphics.glfwWindow.reset();

#ifdef TRACY_ENABLE_GRAPHICS
        for (auto ctx : tracing.tracyContexts)
            if (ctx) TracyVkDestroy(ctx);
#endif
    }

    // Releases old swapchain after creating a new one
    void DeviceContext::CreateSwapchain() {
        ZoneScoped;
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);

        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
            return;
        }
        if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max() ||
            surfaceCapabilities.currentExtent.height == std::numeric_limits<uint32_t>::max()) {
            auto windowSize = CVarWindowSize.Get();
            surfaceCapabilities.currentExtent.width = windowSize.x;
            surfaceCapabilities.currentExtent.height = windowSize.y;
        }

        auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
        auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

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

        auto newSwapchain = device->createSwapchainKHRUnique(swapchainInfo);
        swapchainImageContexts.clear();
        swapchain.swap(newSwapchain);

        auto swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        swapchainImageContexts.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            ImageViewCreateInfo imageViewInfo = {};
            imageViewInfo.image = make_shared<Image>(swapchainImages[i],
                swapchainInfo.imageFormat,
                swapchainInfo.imageExtent);
            imageViewInfo.swapchainLayout = vk::ImageLayout::ePresentSrcKHR;
            swapchainImageContexts[i].imageView = CreateImageView(imageViewInfo);
        }
    }

    void DeviceContext::RecreateSwapchain() {
        ZoneScoped;
        Assertf(swapchain, "DeviceContext::RecreateSwapchain called without existing swapchain");
        device->waitIdle();
        CreateSwapchain();
    }

    void DeviceContext::InitRenderer(Game &game) {
        vkRenderer = make_shared<vulkan::Renderer>(game, *this, graph, *compositor);
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

    void DeviceContext::RenderFrame(chrono_clock::duration elapsedTime) {
        if (vkRenderer) vkRenderer->RenderFrame(elapsedTime);
    }

    void DeviceContext::AttachWindow(const std::shared_ptr<GuiContext> &context) {
        if (!context) return;
        auto *perfTimer = GetPerfTimer();
        if (perfTimer) {
            if (!profilerGui) profilerGui = make_shared<ProfilerGui>(*perfTimer);
            context->Attach(std::static_pointer_cast<ecs::GuiDefinition>(profilerGui));
        }
        if (vkRenderer) vkRenderer->AttachWindow(context);
    }

    bool DeviceContext::BeginFrame() {
        ZoneScoped;
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

            {
                ZoneScopedN("WaitForFrameFence");
                auto result = device->waitForFences({*Frame().inFlightFence}, true, FENCE_WAIT_TIME);
                AssertVKSuccess(result, "timed out waiting for fence");
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

                if (SwapchainImage().inFlightFence) {
                    ZoneScopedN("WaitForImageFence");
                    auto result = device->waitForFences({SwapchainImage().inFlightFence}, true, FENCE_WAIT_TIME);
                    AssertVKSuccess(result, "timed out waiting for fence");
                }
                SwapchainImage().inFlightFence = *Frame().inFlightFence;
            }

            vmaSetCurrentFrameIndex(allocator.get(), frameCounter);
            PrepareResourcesForFrame();

#ifdef TRACY_ENABLE_GRAPHICS
            for (size_t i = 0; i < tracing.tracyContexts.size(); i++) {
                auto prevQueueSubmitFrame = queueLastSubmit[i];
                if (prevQueueSubmitFrame < frameCounter - 1) continue;

                auto trctx = tracing.tracyContexts[i];
                if (!trctx) continue;

                auto ctx = GetFencedCommandContext(CommandContextType(i));
                TracyVkCollect(trctx, ctx->Raw());
                Submit(ctx);

                queueLastSubmit[i] = prevQueueSubmitFrame;
            }
#endif

            frameBeginQueue.Flush();
        } catch (const vk::DeviceLostError &err) {
            Errorf("Device lost! BeginFrame() %s", err.what());
            deviceResetRequired = true;
            return false;
        }
        return true;
    }

    void DeviceContext::PrepareResourcesForFrame() {
        ZoneScoped;
        for (auto &pool : Frame().commandContexts) {
            // Resets all command buffers in the pool, so they can be recorded and used again.
            if (pool.nextIndex > 0) {
                ZoneScopedN("ResetCommandPool");
                device->resetCommandPool(*pool.commandPool);
            }
            pool.nextIndex = 0;
        }

        erase_if(Frame().inFlightObjects, [&](auto &entry) {
            return device->getFenceStatus(entry.fence) == vk::Result::eSuccess;
        });

        Thread().ReleaseAvailableResources();
    }

    void DeviceContext::SwapBuffers() {
        if (!swapchain) return;
        ZoneScoped;

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
        } catch (const vk::OutOfDateKHRError &) {
            RecreateSwapchain();
        }
    }

    void DeviceContext::EndFrame() {
        if (vkRenderer) vkRenderer->EndFrame();

        allocatorQueue.Dispatch<void>([this]() {
            Thread().ReleaseAvailableResources();
        });

        frameEndQueue.Flush();

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
    }

    CommandContextPtr DeviceContext::GetFrameCommandContext(render_graph::Resources &resources,
        CommandContextType type) {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "must use a fenced command context in a single thread");
        }

        CommandContextPtr cmd;
        auto &pool = Frame().commandContexts[QueueType(type)];
        if (pool.nextIndex < pool.list.size()) {
            cmd = pool.list[pool.nextIndex++];

            // Reset cmd to default state
            auto buffer = std::move(cmd->RawRef());
            cmd->~CommandContext();
            new (cmd.get()) CommandContext(*this, std::move(buffer), type, CommandContextScope::Frame);
        } else {
            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.commandPool = *pool.commandPool;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandBufferCount = 1;
            auto buffers = device->allocateCommandBuffersUnique(allocInfo);

            cmd = make_shared<CommandContext>(*this, std::move(buffers[0]), type, CommandContextScope::Frame);
            pool.list.push_back(cmd);
            pool.nextIndex++;
        }
        cmd->Begin(&resources);
        return cmd;
    }

    CommandContextPtr DeviceContext::GetFencedCommandContext(CommandContextType type) {
        auto &thr = Thread();
        auto cmdHandle = thr.commandContexts[QueueType(type)]->Get();
        thr.pendingCommandContexts[QueueType(type)].push_back(cmdHandle);
        cmdHandle.Get()->Begin();
        return cmdHandle;
    }

    void DeviceContext::Submit(CommandContextPtr &cmd,
        vk::ArrayProxy<const vk::Semaphore> signalSemaphores,
        vk::ArrayProxy<const vk::Semaphore> waitSemaphores,
        vk::ArrayProxy<const vk::PipelineStageFlags> waitStages,
        vk::Fence fence,
        bool lastSubmit) {
        Submit({1, &cmd}, signalSemaphores, waitSemaphores, waitStages, fence, lastSubmit);
    }

    void DeviceContext::Submit(vk::ArrayProxyNoTemporaries<CommandContextPtr> cmds,
        vk::ArrayProxy<const vk::Semaphore> signalSemaphores,
        vk::ArrayProxy<const vk::Semaphore> waitSemaphores,
        vk::ArrayProxy<const vk::PipelineStageFlags> waitStages,
        vk::Fence fence,
        bool lastSubmit) {
        ZoneScoped;
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "must call Submit from a single thread");
        }
        Assert(waitSemaphores.size() == waitStages.size(), "must have exactly one wait stage per wait semaphore");

        InlineVector<vk::Semaphore, 8> signalSemArray;
        signalSemArray.insert(signalSemArray.end(), signalSemaphores.begin(), signalSemaphores.end());

        InlineVector<vk::Semaphore, 8> waitSemArray;
        waitSemArray.insert(waitSemArray.end(), waitSemaphores.begin(), waitSemaphores.end());

        InlineVector<vk::PipelineStageFlags, 8> waitStageArray;
        waitStageArray.insert(waitStageArray.end(), waitStages.begin(), waitStages.end());

        QueueType queue = QUEUE_TYPES_COUNT;

        if (lastSubmit) {
            Assert(!fence, "can't use custom fence on frame's last submit call");
            fence = *Frame().inFlightFence;
            device->resetFences({fence});
        }

        for (auto &cmd : cmds) {
            auto cmdFence = cmd->Fence();
            auto cmdQueue = QueueType(cmd->GetType());

            if (cmd->WritesToSwapchain()) {
                waitStageArray.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
                waitSemArray.push_back(*Frame().imageAvailableSemaphore);
                signalSemArray.push_back(*Frame().renderCompleteSemaphore);
                Assert(lastSubmit, "swapchain write must be in the last submit batch of the frame");
            }

            if (cmdFence) {
                Assert(!fence, "can't submit with multiple fences");
                fence = cmdFence;
            }

            Assert(queue == QUEUE_TYPES_COUNT || cmdQueue == queue, "can't submit with multiple queues");
            queue = cmdQueue;
        }

        vector<vk::CommandBuffer> cmdBufs;
        cmdBufs.reserve(cmds.size());
        for (auto &cmd : cmds) {
            if (cmd->recording) cmd->End();
            cmdBufs.push_back(cmd->Raw());
        }

        vk::SubmitInfo submitInfo = {};
        submitInfo.waitSemaphoreCount = waitSemArray.size();
        submitInfo.pWaitSemaphores = waitSemArray.data();
        submitInfo.pWaitDstStageMask = waitStageArray.data();
        submitInfo.signalSemaphoreCount = signalSemArray.size();
        submitInfo.pSignalSemaphores = signalSemArray.data();
        submitInfo.commandBufferCount = cmdBufs.size();
        submitInfo.pCommandBuffers = cmdBufs.data();

        queueLastSubmit[queue] = frameCounter;
        try {
            ZoneScopedN("VkQueueSubmit");
            queues[queue].submit({submitInfo}, fence);
        } catch (const vk::DeviceLostError &err) {
            Errorf("Device lost! queue.submit() %s", err.what());
            deviceResetRequired = true;
        }

        for (auto cmdPtr = cmds.data(); cmdPtr != cmds.end(); cmdPtr++) {
            cmdPtr->reset();
        }
    }

    BufferPtr DeviceContext::AllocateBuffer(BufferLayout layout, vk::BufferUsageFlags usage, VmaMemoryUsage residency) {
        DebugAssert(usage != vk::BufferUsageFlags(), "AllocateBuffer called without usage flags");
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = layout.size;
        bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        return make_shared<Buffer>(bufferInfo, allocInfo, allocator.get(), layout.arrayStride, layout.arrayCount);
    }

    BufferPtr DeviceContext::AllocateBuffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo) {
        return make_shared<Buffer>(bufferInfo, allocInfo, allocator.get());
    }

    BufferPtr DeviceContext::GetBuffer(const BufferDesc &desc) {
        return Thread().bufferPool->Get(desc);
    }

    AsyncPtr<Buffer> DeviceContext::CreateUploadBuffer(const InitialData &data, vk::BufferUsageFlags usage) {
        if (!data.data) return nullptr;
        return allocatorQueue.Dispatch<Buffer>([=, this] {
            vk::BufferCreateInfo bufferInfo = {};
            bufferInfo.size = data.dataSize;
            bufferInfo.usage = usage;
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

            Assertf(data.data, "DeviceContext::CreateBuffer null initial buffer data");
            auto buf = AllocateBuffer(bufferInfo, allocInfo);
            buf->CopyFrom(data.data, data.dataSize);
            return buf;
        });
    }

    AsyncPtr<Buffer> DeviceContext::CreateBuffer(const InitialData &data,
        vk::BufferCreateInfo bufferInfo,
        VmaAllocationCreateInfo allocInfo) {
        return allocatorQueue.Dispatch<Buffer>([=, this]() {
            auto buf = AllocateBuffer(bufferInfo, allocInfo);
            buf->CopyFrom(data.data, data.dataSize);
            return buf;
        });
    }

    AsyncPtr<Buffer> DeviceContext::CreateBuffer(const InitialData &data,
        vk::BufferUsageFlags usage,
        VmaMemoryUsage residency) {
        return allocatorQueue.Dispatch<Buffer>([=, this]() {
            auto buf = AllocateBuffer(data.dataSize, usage, residency);
            buf->CopyFrom(data.data, data.dataSize);
            return buf;
        });
    }

    AsyncPtr<void> DeviceContext::TransferBuffers(vk::ArrayProxy<const BufferTransfer> batch) {
        auto transferCmd = GetFencedCommandContext(CommandContextType::TransferAsync);

        for (auto &transfer : batch) {
            vk::BufferCopy region;
            region.srcOffset = 0;
            region.dstOffset = 0;

            vk::Buffer srcBuf, dstBuf;

            if (std::holds_alternative<SubBufferPtr>(transfer.src)) {
                auto &subBuf = std::get<SubBufferPtr>(transfer.src);
                region.srcOffset = subBuf->ByteOffset();
                region.size = subBuf->ByteSize();
                srcBuf = (vk::Buffer)*subBuf;
            } else {
                auto &buf = std::get<BufferPtr>(transfer.src);
                region.size = buf->ByteSize();
                srcBuf = (vk::Buffer)*buf;
            }

            vk::DeviceSize dstSize;
            if (std::holds_alternative<SubBufferPtr>(transfer.dst)) {
                auto &subBuf = std::get<SubBufferPtr>(transfer.dst);
                region.dstOffset = subBuf->ByteOffset();
                dstSize = subBuf->ByteSize();
                dstBuf = (vk::Buffer)*subBuf;
            } else {
                auto &buf = std::get<BufferPtr>(transfer.dst);
                dstSize = buf->ByteSize();
                dstBuf = (vk::Buffer)*buf;
            }
            Assertf(dstSize == region.size,
                "must transfer between buffers of the same size, src: %llu, dst: %llu",
                region.size,
                dstSize);

            transferCmd->Raw().copyBuffer(srcBuf, dstBuf, {region});
        }

        transferCmd->End();

        return frameEndQueue.Dispatch<void>([this, transferCmd]() {
            auto cmd = transferCmd;
            Submit(cmd);
        });
    }

    ImagePtr DeviceContext::AllocateImage(vk::ImageCreateInfo info,
        VmaMemoryUsage residency,
        vk::ImageUsageFlags declaredUsage) {
        ZoneScoped;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = residency;
        if (!declaredUsage) declaredUsage = info.usage;
        return make_shared<Image>(info, allocInfo, allocator.get(), declaredUsage);
    }

    void transferImageQueueType(DeviceContext &device,
        const std::shared_ptr<Image> &image,
        CommandContextType type = CommandContextType::General,
        Access access = Access::FragmentShaderSampleImage) {
        ImageBarrierInfo transferToQueue = {};
        transferToQueue.srcQueueFamilyIndex = image->LastQueueFamily();
        transferToQueue.dstQueueFamilyIndex = device.QueueFamilyIndex(type);
        auto waitSemaphore = image->GetWaitSemaphore(transferToQueue.dstQueueFamilyIndex);
        if (transferToQueue.srcQueueFamilyIndex == transferToQueue.dstQueueFamilyIndex ||
            transferToQueue.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
            transferToQueue.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            transferToQueue.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        }

        auto cmd = device.GetFencedCommandContext(type);
        auto info = GetAccessInfo(access);
        cmd->ImageBarrier(image, info.imageLayout, info.stageMask, info.accessMask, transferToQueue);
        device.PushInFlightObject(image, cmd->Fence());
        auto transferComplete = image->SetPendingCommand(device.GetEmptySemaphore(cmd->Fence()),
            transferToQueue.dstQueueFamilyIndex);
        if (waitSemaphore) {
            device.Submit(cmd, {transferComplete}, {waitSemaphore}, {info.stageMask});
        } else {
            device.Submit(cmd, {transferComplete}, {}, {});
        }
        image->SetLayout(vk::ImageLayout::eUndefined, info.imageLayout);
    }

    AsyncPtr<Image> DeviceContext::CreateImage(ImageCreateInfo createInfo, const InitialData &initialData) {
        if (!initialData.data) {
            Assert(!createInfo.genMipmap, "DeviceContext::CreateImage must pass initial data to generate a mipmap");
            return CreateImage(createInfo, AsyncPtr<Buffer>());
        } else {
            return CreateImage(createInfo, CreateUploadBuffer(initialData));
        }
    }

    AsyncPtr<Image> DeviceContext::CreateImage(ImageCreateInfo createInfo, const AsyncPtr<Buffer> &uploadBuffer) {
        ZoneScoped;

        bool genMipmap = createInfo.genMipmap;
        bool genFactor = !createInfo.factor.empty();
        vk::ImageUsageFlags declaredUsage = createInfo.usage;
        vk::Format factorFormat = createInfo.format;

        if (!createInfo.mipLevels) createInfo.mipLevels = genMipmap ? CalculateMipmapLevels(createInfo.extent) : 1;
        if (!createInfo.arrayLayers) createInfo.arrayLayers = 1;

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

        auto futImage = allocatorQueue.Dispatch<Image>([this, createInfo, declaredUsage]() {
            auto actualCreateInfo = createInfo.GetVkCreateInfo();
            auto formatInfo = createInfo.GetVkFormatList();
            if (formatInfo.viewFormatCount > 0) actualCreateInfo.pNext = &formatInfo;

            return AllocateImage(actualCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, declaredUsage);
        });

        if (uploadBuffer) {
            futImage = UpdateImage(futImage, uploadBuffer, genMipmap && !genFactor);

            if (genFactor) {
                futImage = frameEndQueue.Dispatch<Image>(futImage, [=, this](ImagePtr image) {
                    if (!image) return std::shared_ptr<Image>();

                    ZoneScopedN("ApplyFactor");
                    auto factorCmd = GetFencedCommandContext(CommandContextType::ComputeAsync);

                    ImageBarrierInfo transferToCompute = {};
                    transferToCompute.srcQueueFamilyIndex = image->LastQueueFamily();
                    transferToCompute.dstQueueFamilyIndex = QueueFamilyIndex(CommandContextType::ComputeAsync);
                    auto waitSemaphore = image->GetWaitSemaphore(transferToCompute.dstQueueFamilyIndex);
                    if (transferToCompute.srcQueueFamilyIndex == transferToCompute.dstQueueFamilyIndex ||
                        transferToCompute.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
                        transferToCompute.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        transferToCompute.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    }

                    {
                        GPUZone(this, factorCmd, "ApplyFactor");

                        factorCmd->ImageBarrier(image,
                            vk::ImageLayout::eGeneral,
                            vk::PipelineStageFlagBits::eComputeShader,
                            vk::AccessFlagBits::eShaderRead,
                            transferToCompute);
                        image->SetAccess(Access::None, Access::ComputeShaderReadStorage);

                        ImageViewCreateInfo factorViewInfo = {};
                        factorViewInfo.image = image;
                        factorViewInfo.format = factorFormat;
                        factorViewInfo.mipLevelCount = 1;
                        factorViewInfo.usage = vk::ImageUsageFlagBits::eStorage;
                        auto factorView = CreateImageView(factorViewInfo);

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
                        PushInFlightObject(factorView, factorCmd->Fence());
                    }
                    auto factorComplete = image->SetPendingCommand(GetEmptySemaphore(factorCmd->Fence()),
                        transferToCompute.dstQueueFamilyIndex);
                    if (waitSemaphore) {
                        Submit(factorCmd,
                            {factorComplete},
                            {waitSemaphore},
                            {vk::PipelineStageFlagBits::eComputeShader});
                    } else {
                        Submit(factorCmd, {factorComplete}, {}, {});
                    }
                    if (!genMipmap) {
                        transferImageQueueType(*this, image);
                    }
                    return image;
                });
            }
            if (genMipmap) futImage = UpdateImageMipmap(futImage);
        }
        return futImage;
    }

    AsyncPtr<Image> DeviceContext::UpdateImage(const AsyncPtr<Image> &dstImage,
        const AsyncPtr<Buffer> &srcBuffer,
        bool updateMipmap) {
        auto futImage = frameEndQueue.Dispatch<Image>(dstImage,
            srcBuffer,
            [updateMipmap, this](ImagePtr image, BufferPtr stagingBuf) {
                if (!image) return std::shared_ptr<Image>();

                ZoneScopedN("PrepareImage");
                auto transferCmd = GetFencedCommandContext(CommandContextType::TransferAsync);

                ImageBarrierInfo transferToTransferAsync = {};
                transferToTransferAsync.srcQueueFamilyIndex = image->LastQueueFamily();
                transferToTransferAsync.dstQueueFamilyIndex = QueueFamilyIndex(CommandContextType::TransferAsync);
                auto waitSemaphore = image->GetWaitSemaphore(transferToTransferAsync.dstQueueFamilyIndex);
                if (transferToTransferAsync.srcQueueFamilyIndex == transferToTransferAsync.dstQueueFamilyIndex ||
                    transferToTransferAsync.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
                    transferToTransferAsync.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    transferToTransferAsync.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                }

                transferCmd->ImageBarrier(image,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    transferToTransferAsync);

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

                PushInFlightObject(stagingBuf, transferCmd->Fence());
                auto transferComplete = image->SetPendingCommand(GetEmptySemaphore(transferCmd->Fence()),
                    transferToTransferAsync.dstQueueFamilyIndex);
                {
                    ZoneScopedN("CopyBufferToImage");
                    if (waitSemaphore) {
                        Submit(transferCmd,
                            {transferComplete},
                            {waitSemaphore},
                            {vk::PipelineStageFlagBits::eTransfer});
                    } else {
                        Submit(transferCmd, {transferComplete}, {}, {});
                    }
                }

                if (!updateMipmap) transferImageQueueType(*this, image);
                return image;
            });
        if (updateMipmap) futImage = UpdateImageMipmap(futImage);
        return futImage;
    }

    AsyncPtr<Image> DeviceContext::UpdateImageMipmap(const AsyncPtr<Image> &image) {
        ZoneScoped;
        return frameEndQueue.Dispatch<Image>(image, [this](ImagePtr image) {
            if (!image) return std::shared_ptr<Image>();
            auto graphicsCmd = GetFencedCommandContext(CommandContextType::General); // TODO: Add GraphicsAsync

            ImageBarrierInfo transferToGeneral = {};
            transferToGeneral.srcQueueFamilyIndex = image->LastQueueFamily();
            transferToGeneral.dstQueueFamilyIndex = QueueFamilyIndex(CommandContextType::General);
            auto waitSemaphore = image->GetWaitSemaphore(transferToGeneral.dstQueueFamilyIndex);
            if (transferToGeneral.srcQueueFamilyIndex == transferToGeneral.dstQueueFamilyIndex ||
                transferToGeneral.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
                transferToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transferToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            {
                GPUZone(this, graphicsCmd, "Mipmap");

                graphicsCmd->ImageBarrier(image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferRead,
                    transferToGeneral);

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

                graphicsCmd->ImageBarrier(image,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eShaderRead);
            }
            PushInFlightObject(image, graphicsCmd->Fence());
            auto mipmapComplete = image->SetPendingCommand(GetEmptySemaphore(graphicsCmd->Fence()),
                transferToGeneral.dstQueueFamilyIndex);
            if (waitSemaphore) {
                Submit(graphicsCmd, {mipmapComplete}, {waitSemaphore}, {vk::PipelineStageFlagBits::eTransfer});
            } else {
                Submit(graphicsCmd, {mipmapComplete}, {}, {});
            }
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

        return make_shared<ImageView>(device->createImageViewUnique(createInfo), info);
    }

    AsyncPtr<ImageView> DeviceContext::CreateImageAndView(const ImageCreateInfo &imageInfo,
        const ImageViewCreateInfo &viewInfo,
        const InitialData &data) {
        ZoneScoped;
        auto futImage = CreateImage(imageInfo, data);

        return allocatorQueue.Dispatch<ImageView>(futImage, [=, this](ImagePtr image) {
            auto viewI = viewInfo;
            viewI.image = image;
            return CreateImageView(viewI);
        });
    }

    AsyncPtr<ImageView> DeviceContext::LoadAssetImage(string_view assetName, bool genMipmap, bool srgb) {
        auto futImage = Assets().LoadImage(assetName);
        return allocatorQueue.Dispatch<ImageView>(futImage,
            [this, name = std::string(assetName), genMipmap, srgb](std::shared_ptr<sp::Image> image) {
                if (!image) {
                    Warnf("Missing asset image: %s", name);
                    return std::make_shared<Async<ImageView>>();
                }
                return LoadImage(image, genMipmap, srgb);
            });
    }

    AsyncPtr<ImageView> DeviceContext::LoadImage(shared_ptr<const sp::Image> image, bool genMipmap, bool srgb) {
        ZoneScoped;
        Assertf(image, "DeviceContext::LoadImage called with null image");
        ImageCreateInfo createInfo;
        createInfo.extent = vk::Extent3D(image->GetWidth(), image->GetHeight(), 1);
        Assert(createInfo.extent.width > 0 && createInfo.extent.height > 0, "image has zero size");

        createInfo.format = FormatFromTraits(image->GetComponents(), 8, srgb);
        Assert(createInfo.format != vk::Format::eUndefined, "invalid image format");

        createInfo.genMipmap = genMipmap;
        createInfo.usage = vk::ImageUsageFlagBits::eSampled;

        const uint8_t *data = image->GetImage().get();
        Assert(data, "missing image data");

        ImageViewCreateInfo viewInfo = {};
        if (genMipmap) {
            viewInfo.defaultSampler = GetSampler(SamplerType::TrilinearTiled);
        } else {
            viewInfo.defaultSampler = GetSampler(SamplerType::BilinearClampEdge);
        }
        return CreateImageAndView(createInfo, viewInfo, {data, image->ByteSize(), image});
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

    ShaderHandle DeviceContext::LoadShader(string_view name) {
        auto it = shaderHandles.find(name);
        if (it != shaderHandles.end()) return it->second;

        string strName(name);
        auto shader = shaders.emplace_back(CreateShader(strName, {}));
        auto handle = static_cast<ShaderHandle>(shaders.size());
        shaderHandles[strName] = handle;
        return handle;
    }

    shared_ptr<Shader> DeviceContext::CreateShader(const string &name, Hash64 compareHash) {
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

        return make_shared<Shader>(name, std::move(shaderModule), std::move(reflection), newHash);
    }

    shared_ptr<Shader> DeviceContext::GetShader(ShaderHandle handle) const {
        if (handle == 0 || shaders.size() < (size_t)handle) return nullptr;

        return shaders[handle - 1];
    }

    shared_ptr<Pipeline> DeviceContext::GetPipeline(const PipelineCompileInput &input) {
        return pipelinePool->GetPipeline(input);
    }

    ImageViewPtr DeviceContext::SwapchainImageView() {
        return swapchain ? SwapchainImage().imageView : nullptr;
    }

    shared_ptr<RenderPass> DeviceContext::GetRenderPass(const RenderPassInfo &info) {
        return renderPassPool->GetRenderPass(info);
    }

    shared_ptr<Framebuffer> DeviceContext::GetFramebuffer(const RenderPassInfo &info) {
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

    SharedHandle<vk::Fence> DeviceContext::GetEmptyFence() {
        return fencePool->Get();
    }

    std::shared_ptr<vk::UniqueSemaphore> DeviceContext::GetEmptySemaphore(vk::Fence inUseUntilFence) {
        // auto sem = semaphorePool->Get();
        vk::SemaphoreCreateInfo semaphoreInfo = {};
        auto sem = std::make_shared<vk::UniqueSemaphore>(device->createSemaphoreUnique(semaphoreInfo));
        PushInFlightObject(sem, inUseUntilFence);
        return sem;
    }

    void DeviceContext::PushInFlightObject(TemporaryObject object, vk::Fence fence) {
        if (!fence) fence = *Frame().inFlightFence;
        Frame().inFlightObjects.emplace_back(InFlightObject{object, fence});
    }

    void DeviceContext::ThreadContext::ReleaseAvailableResources() {
        ZoneScoped;
        for (uint32_t queueType = 0; queueType < QUEUE_TYPES_COUNT; queueType++) {
            erase_if(pendingCommandContexts[queueType], [](auto &cmdHandle) {
                auto &cmd = cmdHandle.Get();
                auto fence = cmd->Fence();
                return !fence || cmd->Device()->getFenceStatus(fence) == vk::Result::eSuccess;
            });
        }

        bufferPool->Tick();
        if (printBufferStats.exchange(false)) bufferPool->LogStats();
    }

    vk::FormatProperties DeviceContext::FormatProperties(vk::Format format) const {
        return physicalDevice.getFormatProperties(format);
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
    tracy::VkCtx *DeviceContext::GetTracyContext(CommandContextType type) {
        return tracing.tracyContexts[(size_t)type];
    }
#endif

    void *DeviceContext::Win32WindowHandle() {
        return graphics.windowHandlers.win32_handle;
    }
} // namespace sp::vulkan
