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

#include "WinitInputHandler.hh"
#include "common/Defer.hh"
#include "common/Logging.hh"

#include <csignal>
#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <memory>
#include <strayphotons.h>
#include <winit.rs.h>

using cxxopts::value;

namespace sp {
    sp_game_t GameInstance = (sp_game_t)0;
    GraphicsManager *GameGraphics = nullptr;

    void handleSignals(int signal) {
        if (signal == SIGINT && GameInstance) {
            sp_game_trigger_exit(GameInstance);
        }
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

    std::shared_ptr<std::remove_pointer_t<sp_game_t>> instance(sp_game_init(ARGC_NAME, ARGV_NAME), [](sp_game_t ctx) {
        GameInstance = nullptr;
        sp_game_destroy(ctx);
    });

    GameInstance = instance.get();
    if (!GameInstance) return 1;

    GameGraphics = sp_game_get_graphics_manager(GameInstance);

    glm::ivec2 initialSize;
    sp_cvar_t *cvarWindowSize = sp_get_cvar("r.size");
    sp_cvar_get_ivec2(cvarWindowSize, &initialSize.x, &initialSize.y);
    bool enableValidationLayers = sp_game_get_cli_flag(GameInstance, "with-validation-layers");
    auto *winitContext = winit::create_context(initialSize.x, initialSize.y, enableValidationLayers).into_raw();
    Assert(winitContext, "winit context creation failed");
    sp_graphics_set_winit_context(GameGraphics, winitContext, [](winit::WinitContext *ptr) {
        if (ptr) winit::destroy_window(*ptr);

        // Construct rust Box<> from pointer and let its destructor run.
        (void)rust::cxxbridge1::Box<winit::WinitContext>::from_raw(ptr);
    });

    VkInstance vkInstance = (VkInstance)winit::get_instance_handle(*winitContext);
    Assert(vkInstance, "winit instance creation failed");
    sp_graphics_set_vulkan_instance(GameGraphics, vkInstance, [](GraphicsManager *graphics, VkInstance instance) {
        Tracef("Destroying rust instance");
        auto *winitContext = sp_graphics_get_winit_context(graphics);
        if (winitContext) winit::destroy_instance(*winitContext);
    });

    VkSurfaceKHR vkSurface = (VkSurfaceKHR)winit::get_surface_handle(*winitContext);
    Assert(vkSurface, "winit window surface creation failed");
    sp_graphics_set_vulkan_surface(GameGraphics, vkSurface, [](GraphicsManager *graphics, VkSurfaceKHR surface) {
        Tracef("Destroying rust surface");
        auto *winitContext = sp_graphics_get_winit_context(graphics);
        if (winitContext) winit::destroy_surface(*winitContext);
    });

    WinitInputHandler *inputHandler = new WinitInputHandler(GameInstance, winitContext);
    sp_game_set_input_handler(GameInstance, inputHandler, [](void *handler) {
        auto *inputHandler = (WinitInputHandler *)handler;
        delete inputHandler;
    });

    sp_window_handlers_t windowHandlers;
    windowHandlers.get_video_modes = [](GraphicsManager *graphics, size_t *mode_count_out, sp_video_mode_t *modes_out) {
        Assertf(mode_count_out != nullptr, "windowHandlers.get_video_modes called with null count pointer");
        winit::WinitContext *winitContext = sp_graphics_get_winit_context(graphics);
        if (!winitContext) {
            Warnf("Failed to read Winit monitor modes");
            *mode_count_out = 0;
            return;
        }
        auto modes = winit::get_monitor_modes(*winitContext);
        if (modes_out && *mode_count_out >= modes.size()) {
            for (size_t i = 0; i < modes.size(); i++) {
                modes_out[i] = {modes[i].width, modes[i].height};
            }
        }
        *mode_count_out = modes.size();
    };
    windowHandlers.set_title = [](GraphicsManager *graphics, const char *title) {
        winit::WinitContext *winitContext = sp_graphics_get_winit_context(graphics);
        if (winitContext) winit::set_window_title(*winitContext, title);
    };
    // windowHandlers.should_close = [](GraphicsManager *graphics) {
    //     winit::WinitContext *winitContext = sp_graphics_get_winit_context(graphics);
    //     return window && !!glfwWindowShouldClose(window);
    // };
    windowHandlers.update_window_view = [](GraphicsManager *graphics, int *width_out, int *height_out) {
        winit::WinitContext *winitContext = sp_graphics_get_winit_context(graphics);
        if (!winitContext) return;

        static bool systemFullscreen;
        static glm::ivec2 systemWindowSize;
        static glm::ivec4 storedWindowRect; // Remember window position and size when returning from fullscreen

        sp_cvar_t *cvarWindowFullscreen = sp_get_cvar("r.fullscreen");
        sp_cvar_t *cvarWindowSize = sp_get_cvar("r.size");
        bool fullscreen = sp_cvar_get_bool(cvarWindowFullscreen);
        if (systemFullscreen != fullscreen) {
            if (fullscreen) {
                winit::get_window_position(*winitContext, &storedWindowRect.x, &storedWindowRect.y);
                storedWindowRect.z = systemWindowSize.x;
                storedWindowRect.w = systemWindowSize.y;

                auto monitor = winit::get_active_monitor(*winitContext);
                glm::uvec2 readWindowSize;
                winit::get_monitor_resolution(*monitor, &readWindowSize.x, &readWindowSize.y);
                if (readWindowSize != glm::uvec2(0)) systemWindowSize = readWindowSize;
                winit::set_window_mode(*winitContext,
                    &*monitor,
                    0,
                    0,
                    (uint32_t)systemWindowSize.x,
                    (uint32_t)systemWindowSize.y);
            } else {
                systemWindowSize = {storedWindowRect.z, storedWindowRect.w};
                winit::set_window_mode(*winitContext,
                    0,
                    storedWindowRect.x,
                    storedWindowRect.y,
                    (uint32_t)storedWindowRect.z,
                    (uint32_t)storedWindowRect.w);
            }
            sp_cvar_set_ivec2(cvarWindowSize, systemWindowSize.x, systemWindowSize.y);
            systemFullscreen = fullscreen;
        }

        glm::ivec2 windowSize;
        sp_cvar_get_ivec2(cvarWindowSize, &windowSize.x, &windowSize.y);
        if (systemWindowSize != windowSize) {
            if (sp_cvar_get_bool(cvarWindowFullscreen)) {
                auto monitor = winit::get_active_monitor(*winitContext);
                winit::set_window_mode(*winitContext, &*monitor, 0, 0, windowSize.x, windowSize.y);
            } else {
                winit::set_window_inner_size(*winitContext, windowSize.x, windowSize.y);
            }

            systemWindowSize = windowSize;
        }

        glm::uvec2 fbExtents;
        winit::get_window_inner_size(*winitContext, &fbExtents.x, &fbExtents.y);
        if (fbExtents.x > 0 && fbExtents.y > 0) {
            *width_out = fbExtents.x;
            *height_out = fbExtents.y;
        }
    };
    windowHandlers.set_cursor_visible = [](GraphicsManager *graphics, bool visible) {
        winit::WinitContext *winitContext = sp_graphics_get_winit_context(graphics);
        if (!winitContext) return;
        if (visible) {
            winit::set_input_mode(*winitContext, InputMode::CursorNormal);
        } else {
            winit::set_input_mode(*winitContext, InputMode::CursorDisabled);
        }
    };
    windowHandlers.win32_handle = (void *)winit::get_win32_window_handle(*winitContext);
    sp_graphics_set_window_handlers(GameGraphics, &windowHandlers);
    Defer disableHanlders([&] {
        sp_graphics_set_window_handlers(GameGraphics, nullptr);
    });

    int status_code = sp_game_start(GameInstance);
    if (status_code) return status_code;

    if (GameGraphics) {
        inputHandler->StartEventLoop((uint32_t)MaxInputPollRate);
        return sp_game_get_exit_code(GameInstance);
    } else {
        return sp_game_wait_for_exit_trigger(GameInstance);
    }
}
