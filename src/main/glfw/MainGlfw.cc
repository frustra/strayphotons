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

#define VULKAN_HPP_DEFAULT_DISPATCHER (*sp_get_vulkan_dispatch_loader())

#include "GlfwInputHandler.hh"
#include "common/Common.hh"
#include "common/Defer.hh"
#include "common/Logging.hh"

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
#include <fstream>
#include <memory>
#include <strayphotons.h>
#include <vulkan/vulkan.hpp>

using cxxopts::value;

namespace sp {
    sp_game_t *GameInstance = nullptr;
    sp_graphics_ctx_t *GameGraphics = nullptr;
    std::shared_ptr<GlfwInputHandler> GameInputHandler;
    std::atomic_uint64_t GraphicsStepCount, GraphicsMaxStepCount;

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

#if defined(_WIN32) && defined(SP_PACKAGE_RELEASE)
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    int argc = 0;
    wchar_t **wargv = CommandLineToArgvW(pCmdLine, &argc);
    std::vector<std::string> argStore(argc);
    std::vector<char *> argPtrs(argc);
    for (size_t i = 0; i < argStore.size(); i++) {
        std::wstring tmp = wargv[i];
        argStore[i] = std::string(tmp.begin(), tmp.end());
        argPtrs[i] = argStore[i].data();
    }
    char **argv = argPtrs.data();
#else
int main(int argc, char **argv) {
#endif

#ifdef _WIN32
    signal(SIGINT, sp::handleSignals);
#else
    struct sigaction act;
    act.sa_handler = sp::handleSignals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0);
#endif

    std::shared_ptr<std::remove_pointer_t<sp_game_t>> instance(sp_game_init(argc, argv), [](sp_game_t *ctx) {
        if (GameInputHandler) GameInputHandler.reset();
        GameInstance = nullptr;
        if (ctx) sp_game_destroy(ctx);
        glfwTerminate();
    });

    GameInstance = instance.get();
    if (!GameInstance) return 1;

#ifdef SP_PACKAGE_RELEASE
    if (!sp_get_log_output_file()) {
        std::ofstream("./strayphotons.log", std::ios::out | std::ios::trunc); // Clear log file
        sp_set_log_output_file("./strayphotons.log");
    }
#endif

    if (!sp_game_get_cli_flag(GameInstance, "headless")) {
        GameGraphics = sp_game_get_graphics_context(GameInstance);

#ifdef SP_GRAPHICS_SUPPORT_HEADLESS
        sp_cvar_t *maxFpsCvar = sp_get_cvar("r.MaxFPS");
        sp_cvar_set_uint32(maxFpsCvar, 90);
#else
        if (!sp_game_get_cli_flag(GameInstance, "no-vr")) sp_game_enable_xr_system(GameInstance, true);
#endif

        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit()) {
            Errorf("glfwInit() failed");
            return 1;
        }
        Assert(glfwVulkanSupported(), "Vulkan not supported");

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // Disable OpenGL context creation
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        std::vector<const char *> layers;
        if (sp_game_get_cli_flag(GameInstance, "with-validation-layers")) {
            Logf("Running with Vulkan validation layer");
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

        std::vector<const char *> extensions;

        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        // Debugf("Available Vulkan extensions: %u", availableExtensions.size());
        for (auto &ext : availableExtensions) {
            string_view name(ext.extensionName.data());
            // Debugf("\t%s", name);

            if (name == VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) {
                extensions.push_back(name.data());
            } else if (name == VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) {
                extensions.push_back(name.data());
            }
        }
        extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // TODO: Match these extensions with Rust winit.rs
        uint32_t requiredExtensionCount = 0;
        auto requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
        for (uint32_t i = 0; i < requiredExtensionCount; i++) {
            extensions.emplace_back(requiredExtensions[i]);
        }

#ifndef SP_GRAPHICS_SUPPORT_HEADLESS
        // Create window and surface
        glm::ivec2 initialSize;
        sp_cvar_t *cvarWindowSize = sp_get_cvar("r.size");
        sp_cvar_get_ivec2(cvarWindowSize, &initialSize.x, &initialSize.y);
        GLFWwindow *window = glfwCreateWindow(initialSize.x, initialSize.y, "STRAY PHOTONS", nullptr, nullptr);
        Assert(window, "glfw window creation failed");
        sp_graphics_set_glfw_window(GameGraphics, window, [](GLFWwindow *window) {
            if (window) glfwDestroyWindow(window);
        });
#endif

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
#ifdef SP_DEBUG
        debugInfo.messageSeverity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
#endif
        debugInfo.pfnUserCallback = &VulkanDebugCallback;

        createInfo.setPNext((VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo);

        vk::Instance vkInstance = vk::createInstance(createInfo);
        sp_graphics_set_vulkan_instance(GameGraphics, vkInstance, [](sp_graphics_ctx_t *graphics, VkInstance instance) {
            if (instance) ((vk::Instance)instance).destroy();
        });

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkInstance);

#ifndef SP_GRAPHICS_SUPPORT_HEADLESS
        vk::SurfaceKHR vkSurface;
        VkResult result = glfwCreateWindowSurface(vkInstance, window, nullptr, (VkSurfaceKHR *)&vkSurface);
        if (result != VK_SUCCESS) {
            Abortf("creating window surface (%s)", vk::to_string(static_cast<vk::Result>(result)));
        }
        Assert(vkSurface, "gkfw window surface creation failed");
        sp_graphics_set_vulkan_surface(GameGraphics, vkSurface, [](sp_graphics_ctx_t *graphics, VkSurfaceKHR surface) {
            if (graphics && surface) {
                vk::Instance instance = sp_graphics_get_vulkan_instance(graphics);
                if (instance) instance.destroySurfaceKHR(surface);
            }
        });

        GameInputHandler = std::make_shared<GlfwInputHandler>(GameInstance, window);
#endif

        sp_window_handlers_t windowHandlers = {0};
        windowHandlers.get_video_modes =
            [](sp_graphics_ctx_t *graphics, size_t *mode_count_out, sp_video_mode_t *modes_out) {
                Assertf(mode_count_out != nullptr, "windowHandlers.get_video_modes called with null count pointer");
                int modeCount;
                const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &modeCount);
                if (!modes || modeCount <= 0) {
                    Warnf("Failed to read Glfw monitor modes");
                    *mode_count_out = 0;
                    return;
                }
                if (modes_out && *mode_count_out >= (size_t)modeCount) {
                    for (size_t i = 0; i < (size_t)modeCount; i++) {
                        modes_out[i] = {(uint32_t)modes[i].width, (uint32_t)modes[i].height};
                    }
                }
                *mode_count_out = modeCount;
            };
#ifndef SP_GRAPHICS_SUPPORT_HEADLESS
        windowHandlers.set_title = [](sp_graphics_ctx_t *graphics, const char *title) {
            GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
            if (window) glfwSetWindowTitle(window, title);
        };
        windowHandlers.should_close = [](sp_graphics_ctx_t *graphics) {
            GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
            return window && !!glfwWindowShouldClose(window);
        };
        windowHandlers.update_window_view = [](sp_graphics_ctx_t *graphics, int *width_out, int *height_out) {
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
        windowHandlers.set_cursor_visible = [](sp_graphics_ctx_t *graphics, bool visible) {
            GLFWwindow *window = sp_graphics_get_glfw_window(graphics);
            if (!window) return;
            if (visible) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        };
    #ifdef _WIN32
        windowHandlers.win32_handle = glfwGetWin32Window(window);
    #endif
#endif
        sp_graphics_set_window_handlers(GameGraphics, &windowHandlers);
    }

    Defer disableHanlders([&] {
        sp_graphics_set_window_handlers(GameGraphics, nullptr);
    });

    int status_code = sp_game_start(GameInstance);
    if (status_code) return status_code;

    if (GameGraphics) {
        bool scriptMode = sp_game_get_cli_flag(GameInstance, "run");
        sp_cvar_t *cfuncStepGraphics = nullptr;
        if (scriptMode) {
            cfuncStepGraphics = sp_register_cfunc_uint32("stepgraphics",
                "Renders N frames in a row, saving any queued screenshots, default is 1",
                [](unsigned int arg) {
                    auto count = std::max(1u, arg);
                    for (auto i = 0u; i < count; i++) {
                        // Step main thread glfw input first
                        GraphicsMaxStepCount++;
                        auto step = GraphicsStepCount.load();
                        while (step < GraphicsMaxStepCount) {
                            GraphicsStepCount.wait(step);
                            step = GraphicsStepCount.load();
                        }

                        sp_graphics_step_thread(GameGraphics, 1);
                    }
                });
        }
        Defer unregisterStepFunc([&] {
            if (cfuncStepGraphics) sp_unregister_cfunc(cfuncStepGraphics);
        });

        sp_cvar_t *cvarMaxFps = sp_get_cvar("r.maxfps");

        auto frameEnd = chrono_clock::now();
        while (!sp_game_is_exit_triggered(GameInstance)) {
            static const char *frameName = "WindowInput";
            (void)frameName;
            FrameMarkStart(frameName);
            if (scriptMode) {
                while (GraphicsStepCount < GraphicsMaxStepCount) {
                    GlfwInputHandler::Frame();
                    sp_graphics_handle_input_frame(GameGraphics);
                    GraphicsStepCount++;
                }
                GraphicsStepCount.notify_all();
            } else {
                GlfwInputHandler::Frame();
                if (!sp_graphics_handle_input_frame(GameGraphics)) {
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
