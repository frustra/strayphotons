#include "VulkanGraphicsContext.hh"

#include "core/Logging.hh"

#include <algorithm>
#include <iostream>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sp {
    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                        void *pUserData) {
        std::cerr << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
        std::cerr << "\t"
                  << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
        std::cerr << "\t"
                  << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
        std::cerr << "\t"
                  << "message         = <" << pCallbackData->pMessage << ">\n";
        if (0 < pCallbackData->queueLabelCount) {
            std::cerr << "\t"
                      << "Queue Labels:\n";
            for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++) {
                std::cerr << "\t\t"
                          << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
            }
        }
        if (0 < pCallbackData->cmdBufLabelCount) {
            std::cerr << "\t"
                      << "CommandBuffer Labels:\n";
            for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
                std::cerr << "\t\t"
                          << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
            }
        }
        if (0 < pCallbackData->objectCount) {
            std::cerr << "\t"
                      << "Objects:\n";
            for (uint8_t i = 0; i < pCallbackData->objectCount; i++) {
                std::cerr << "\t\t"
                          << "Object " << i << "\n";
                std::cerr << "\t\t\t"
                          << "objectType   = "
                          << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
                std::cerr << "\t\t\t"
                          << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
                if (pCallbackData->pObjects[i].pObjectName) {
                    std::cerr << "\t\t\t"
                              << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
                }
            }
        }
        return VK_TRUE;
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

        uint32_t requiredExtensionCount = 0;
        auto requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);

        std::vector<const char *> extensions;
        for (uint32_t i = 0; i < requiredExtensionCount; i++) {
            extensions.emplace_back(requiredExtensions[i]);
        }
        // extensions.emplace_back("VK_KHR_surface");
        // extensions.emplace_back("VK_KHR_get_surface_capabilities2");

        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        Logf("Available Vulkan extensions: %u", availableExtensions.size());
        for (auto &ext : availableExtensions) {
            extensions.emplace_back(ext.extensionName);
            Logf("\t%s %u", ext.extensionName, ext.specVersion);
        }

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
                                          0,
                                          nullptr,
                                          extensions.size(),
                                          extensions.data());
        instance = vk::createInstance(createInfo);
        auto dispatcher = vk::DispatchLoaderDynamic(instance, &glfwGetInstanceProcAddress);
        vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
        debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
        debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
        debugInfo.pfnUserCallback = &VulkanDebugCallback;
        instance.createDebugUtilsMessengerEXT(debugInfo, nullptr, dispatcher);
        vk::DisplaySurfaceCreateInfoKHR surfaceInfo;
        vk::SurfaceKHR surface = instance.createDisplayPlaneSurfaceKHR(surfaceInfo, nullptr, dispatcher);
        Assert(surface, "Failed to create window surface");
        // auto result = glfwCreateWindowSurface(instance, window, nullptr, (VkSurfaceKHR *)&surface);
        // Assert(result == VK_SUCCESS, "Failed to create window surface");

        // vk::InstanceCreateInfo createInfo("hello");
        // instance = vk::raii::Instance(context, );
        // instance = vk::raii::su::makeInstance(context, "STRAY PHOTONS", "sp-engine");
        // #if SP_DEBUG
        //         vk::raii::DebugUtilsMessengerEXT debugUtilsMessenger(instance,
        //         vk::su::makeDebugUtilsMessengerCreateInfoEXT());
        // #endif
        //         physicalDevice = std::move(vk::raii::PhysicalDevices(instance).front());
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
