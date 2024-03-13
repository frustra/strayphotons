/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"
#include "game.h"

#ifdef __cplusplus

namespace sp {
    class GraphicsManager;
} // namespace sp

namespace sp::winit {
    struct WinitContext;
}

extern "C" {
using namespace sp;

typedef sp::winit::WinitContext sp_winit_ctx_t;
#else
typedef void GraphicsManager;
typedef void sp_winit_ctx_t;
#endif

typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;

struct GLFWwindow;

// The following functions are declared in src/exports/Graphics.cc

SP_EXPORT GraphicsManager *sp_game_get_graphics_manager(sp_game_t game);

SP_EXPORT void sp_graphics_set_vulkan_instance(GraphicsManager *graphics,
    VkInstance instance,
    void (*destroy_callback)(GraphicsManager *, VkInstance) = 0);
SP_EXPORT VkInstance sp_graphics_get_vulkan_instance(GraphicsManager *graphics);

SP_EXPORT void sp_graphics_set_vulkan_surface(GraphicsManager *graphics,
    VkSurfaceKHR surface,
    void (*destroy_callback)(GraphicsManager *, VkSurfaceKHR) = 0);
SP_EXPORT VkSurfaceKHR sp_graphics_get_vulkan_surface(GraphicsManager *graphics);

SP_EXPORT void sp_graphics_set_glfw_window(GraphicsManager *graphics,
    GLFWwindow *window,
    void (*destroy_callback)(GLFWwindow *) = 0);
SP_EXPORT GLFWwindow *sp_graphics_get_glfw_window(GraphicsManager *graphics);

SP_EXPORT void sp_graphics_set_winit_context(GraphicsManager *graphics,
    sp_winit_ctx_t *window,
    void (*destroy_callback)(sp_winit_ctx_t *) = 0);
SP_EXPORT sp_winit_ctx_t *sp_graphics_get_winit_context(GraphicsManager *graphics);

typedef struct sp_window_handlers_t {
    // TODO: Get video modes
    void (*set_title)(GraphicsManager *, const char *) = 0;
    bool (*should_close)(GraphicsManager *) = 0;
    void (*update_window_view)(GraphicsManager *, int *, int *) = 0;
    void (*set_cursor_visible)(GraphicsManager *, bool) = 0;
    void *win32_handle = 0;
} sp_window_handlers_t;

SP_EXPORT void sp_graphics_set_window_handlers(GraphicsManager *graphics, const sp_window_handlers_t *handlers);
SP_EXPORT bool sp_graphics_handle_input_frame(GraphicsManager *graphics);

#ifdef __cplusplus
}
#endif
