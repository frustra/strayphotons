/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifdef _WIN32
    #include <windows.h>
#endif

#include <iostream>
using namespace std;

#include "GlfwInputHandler.hh"
#include "common/Common.hh"
#include "common/Defer.hh"
#include "common/Logging.hh"

#ifdef SP_AUDIO_SUPPORT
    #include "audio/AudioManager.hh"
#endif

#ifdef SP_XR_SUPPORT
    #include "xr/XrManager.hh"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <glfw/glfw3native.h>
#endif

#include <csignal>
#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <memory>
#include <strayphotons.h>
#include <vulkan/vulkan.hpp>

using cxxopts::value;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace sp {
    sp_game_t GameInstance = (sp_game_t)0;

    void handleSignals(int signal) {
        if (signal == SIGINT && GameInstance) {
            sp_game_trigger_exit(GameInstance);
        }
    }

    void glfwErrorCallback(int error, const char *message) {
        Errorf("GLFW returned %d: %s", error, message);
    }

    static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pContext) {
        auto typeStr = vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageType));
        string_view message(pCallbackData->pMessage);

        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            if (message.find("CoreValidation-DrawState-QueryNotReset") != string_view::npos) break;
            if (message.find("(subresource: aspectMask 0x1 array layer 0, mip level 0) to be in layout "
                             "VK_IMAGE_LAYOUT_GENERAL--instead, current layout is VK_IMAGE_LAYOUT_PREINITIALIZED.") !=
                string_view::npos)
                break;
            Errorf("VK %s %s", typeStr, message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) break;
            Warnf("VK %s %s", typeStr, message);
            break;
        default:
            break;
        }
        Tracef("VK %s %s", typeStr, message);
        return VK_FALSE;
    }
} // namespace sp

using namespace sp;

const int64_t MaxInputPollRate = 144;

// TODO: Commented until package release saves a log file
// #if defined(_WIN32) && defined(SP_PACKAGE_RELEASE)
//     #define ARGC_NAME __argc
//     #define ARGV_NAME __argv
// int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
// #else
#define ARGC_NAME argc
#define ARGV_NAME argv
int main(int argc, char **argv)
// #endif
{

#ifdef _WIN32
    signal(SIGINT, sp::handleSignals);
#else
    struct sigaction act;
    act.sa_handler = sp::handleSignals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0);
#endif

    std::shared_ptr<std::remove_pointer_t<sp_game_t>> instance(sp_game_init(ARGC_NAME, ARGV_NAME), [](sp_game_t game) {
        GameInstance = nullptr;
        sp_game_destroy(game);
    });

    GameInstance = instance.get();

    cxxopts::ParseResult *options = sp_game_get_options(GameInstance);
    GraphicsManager *graphicsManager = sp_game_get_graphics_manager(GameInstance);

    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) Abort("glfwInit() failed");
    Assert(glfwVulkanSupported(), "Vulkan not supported");

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Disable OpenGL context creation
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

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

    std::vector<const char *> layers;
    if (options->count("with-validation-layers")) {
        Logf("Running with Vulkan validation layer");
        layers.emplace_back("VK_LAYER_KHRONOS_validation");
    }

    std::vector<const char *> extensions;
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
        } else {
            continue;
        }
        extensions.push_back(name.data());
    }
    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // TODO: Match these extensions with Rust winit.rs
    uint32_t requiredExtensionCount = 0;
    auto requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    for (uint32_t i = 0; i < requiredExtensionCount; i++) {
        extensions.emplace_back(requiredExtensions[i]);
    }

    // Create window and surface
    glm::ivec2 initialSize;
    sp_cvar_t *cvarWindowSize = sp_get_cvar("r.size");
    sp_cvar_get_ivec2(cvarWindowSize, &initialSize.x, &initialSize.y);
    GLFWwindow *window = glfwCreateWindow(initialSize.x, initialSize.y, "STRAY PHOTONS", nullptr, nullptr);
    Assert(window, "glfw window creation failed");
    sp_graphics_set_glfw_window(graphicsManager, window, [](GLFWwindow *window) {
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    });

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
        extensions.data(),
        (VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo);

    vk::Instance vkInstance = vk::createInstance(createInfo);
    sp_graphics_set_vulkan_instance(graphicsManager, vkInstance, [](GraphicsManager *graphics, VkInstance instance) {
        if (instance) ((vk::Instance)instance).destroy();
    });

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkInstance);

    vk::SurfaceKHR vkSurface;
    VkResult result = glfwCreateWindowSurface(vkInstance, window, nullptr, (VkSurfaceKHR *)&vkSurface);
    if (result != VK_SUCCESS) {
        Abortf("creating window surface (%s)", vk::to_string(static_cast<vk::Result>(result)));
    }
    Assert(vkSurface, "gkfw window surface creation failed");
    sp_graphics_set_vulkan_surface(graphicsManager, vkSurface, [](GraphicsManager *graphics, VkSurfaceKHR surface) {
        if (graphics && surface) {
            vk::Instance instance = sp_graphics_get_vulkan_instance(graphics);
            if (instance) instance.destroySurfaceKHR(surface);
        }
    });

    GlfwInputHandler *inputHandler = new GlfwInputHandler(GameInstance, window);
    sp_game_set_input_handler(GameInstance, inputHandler, [](void *handler) {
        auto *inputHandler = (GlfwInputHandler *)handler;
        delete inputHandler;
    });

    sp_window_handlers_t windowHandlers;
    windowHandlers.get_video_modes = [](GraphicsManager *graphics, int *mode_count_out, sp_video_mode_t *modes_out) {
        Assertf(mode_count_out != nullptr, "windowHandlers.get_video_modes called with null count pointer");
        int modeCount;
        const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &modeCount);
        if (!modes || modeCount <= 0) {
            Warnf("Failed to read Glfw monitor modes");
            *mode_count_out = 0;
            return;
        }
        if (modes_out && *mode_count_out >= modeCount) {
            for (int i = 0; i < modeCount; i++) {
                modes_out[i] = {modes[i].width, modes[i].height};
            }
        }
        *mode_count_out = modeCount;
    };
    windowHandlers.set_title = [](GraphicsManager *graphics, const char *title) {
        GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
        if (window) glfwSetWindowTitle(window, title);
    };
    windowHandlers.should_close = [](GraphicsManager *graphics) {
        GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
        return window && !!glfwWindowShouldClose(window);
    };
    windowHandlers.update_window_view = [](GraphicsManager *graphics, int *width_out, int *height_out) {
        GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
        if (!window) return;

        static bool systemFullscreen;
        static glm::ivec2 systemWindowSize;
        static glm::ivec4 storedWindowRect; // Remember window position and size when returning from fullscreen

        sp_cvar_t *cvarWindowFullscreen = sp_get_cvar("r.fullscreen");
        sp_cvar_t *cvarWindowSize = sp_get_cvar("r.size");
        bool fullscreen = sp_cvar_get_bool(cvarWindowFullscreen);
        if (systemFullscreen != fullscreen) {
            if (fullscreen) {
                glfwGetWindowPos(window, &storedWindowRect.x, &storedWindowRect.y);
                storedWindowRect.z = systemWindowSize.x;
                storedWindowRect.w = systemWindowSize.y;

                auto *monitor = glfwGetPrimaryMonitor();
                if (monitor) {
                    auto *mode = glfwGetVideoMode(monitor);
                    systemWindowSize = {mode->width, mode->height};
                }
                glfwSetWindowMonitor(window, monitor, 0, 0, systemWindowSize.x, systemWindowSize.y, 60);
            } else {
                systemWindowSize = {storedWindowRect.z, storedWindowRect.w};
                glfwSetWindowMonitor(window,
                    nullptr,
                    storedWindowRect.x,
                    storedWindowRect.y,
                    storedWindowRect.z,
                    storedWindowRect.w,
                    0);
            }
            sp_cvar_set_ivec2(cvarWindowSize, systemWindowSize.x, systemWindowSize.y);
            systemFullscreen = fullscreen;
        }

        glm::ivec2 windowSize;
        sp_cvar_get_ivec2(cvarWindowSize, &windowSize.x, &windowSize.y);
        if (systemWindowSize != windowSize) {
            if (sp_cvar_get_bool(cvarWindowFullscreen)) {
                glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, windowSize.x, windowSize.y, 60);
            } else {
                glfwSetWindowSize(window, windowSize.x, windowSize.y);
            }

            systemWindowSize = windowSize;
        }

        glm::ivec2 fbExtents;
        glfwGetFramebufferSize(window, &fbExtents.x, &fbExtents.y);
        if (fbExtents.x > 0 && fbExtents.y > 0) {
            *width_out = fbExtents.x;
            *height_out = fbExtents.y;
        }
    };
    windowHandlers.set_cursor_visible = [](GraphicsManager *graphics, bool visible) {
        GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
        if (!window) return;
        if (visible) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    };
    windowHandlers.win32_handle = glfwGetWin32Window(window);
    sp_graphics_set_window_handlers(graphicsManager, &windowHandlers);

    // #ifdef SP_AUDIO_SUPPORT
    //     game.audio = make_shared<AudioManager>();
    // #endif

    // #ifdef SP_XR_SUPPORT
    //     if (!options.count("no-vr")) {
    //         game.xr = make_shared<xr::XrManager>(&game);
    //         game.xr->LoadXrSystem();
    //     }
    // #endif

    int status_code = sp_game_start(GameInstance);
    if (status_code) return status_code;

    if (graphicsManager) {
        std::atomic_uint64_t graphicsStepCount, graphicsMaxStepCount;

        bool scriptMode = options->count("run") > 0;
        // if (scriptMode) {
        //     game.funcs.Register<unsigned int>("stepgraphics",
        //         "Renders N frames in a row, saving any queued screenshots, default is 1",
        //         [&game](unsigned int arg) {
        //             auto count = std::max(1u, arg);
        //             for (auto i = 0u; i < count; i++) {
        //                 // Step main thread glfw input first
        //                 graphicsMaxStepCount++;
        //                 auto step = graphicsStepCount.load();
        //                 while (step < graphicsMaxStepCount) {
        //                     graphicsStepCount.wait(step);
        //                     step = graphicsStepCount.load();
        //                 }

        //                 graphicsManager->Step(1);
        //             }
        //         });
        // }

        sp_cvar_t *cvarMaxFps = sp_get_cvar("r.maxfps");

        auto frameEnd = chrono_clock::now();
        while (!sp_game_is_exit_triggered(GameInstance)) {
            static const char *frameName = "WindowInput";
            (void)frameName;
            FrameMarkStart(frameName);
            if (scriptMode) {
                while (graphicsStepCount < graphicsMaxStepCount) {
                    GlfwInputHandler::Frame();
                    sp_graphics_handle_input_frame(graphicsManager);
                    graphicsStepCount++;
                }
                graphicsStepCount.notify_all();
            } else {
                GlfwInputHandler::Frame();
                if (!sp_graphics_handle_input_frame(graphicsManager)) {
                    Tracef("Exit triggered via window manager");
                    break;
                }
            }

            auto realFrameEnd = chrono_clock::now();
            chrono_clock::duration interval(0);
            uint32_t maxFps = sp_cvar_get_uint32(cvarMaxFps);
            if (maxFps > 0) {
                interval = std::chrono::nanoseconds((int64_t)(1e9 / (int64_t)maxFps));
            } else {
                interval = std::chrono::nanoseconds((int64_t)(1e9 / MaxInputPollRate));
            }

            frameEnd += interval;

            if (realFrameEnd >= frameEnd) {
                // Falling behind, reset target frame end time.
                // Add some extra time to allow other threads to start transactions.
                frameEnd = realFrameEnd + std::chrono::nanoseconds(100);
            }

            std::this_thread::sleep_until(frameEnd);
            FrameMarkEnd(frameName);
        }
        return sp_game_get_exit_code(GameInstance);
    } else {
        return sp_game_wait_for_exit_trigger(GameInstance);
    }
}
