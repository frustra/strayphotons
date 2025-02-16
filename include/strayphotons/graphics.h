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
    #include <cstddef>
    #include <cstdint>

namespace sp {
    class GraphicsManager;
}

namespace vk::detail {
    class DispatchLoaderDynamic;
}

namespace sp::winit {
    struct WinitContext;
}

extern "C" {
typedef sp::GraphicsManager sp_graphics_ctx_t;
typedef sp::winit::WinitContext sp_winit_ctx_t;
typedef vk::detail::DispatchLoaderDynamic sp_vk_dispatch_loader_t;
#else
    #include <stddef.h>
    #include <stdint.h>

typedef void sp_graphics_ctx_t;
typedef void sp_winit_ctx_t;
typedef void sp_vk_dispatch_loader_t;
#endif

typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;

typedef struct GLFWwindow GLFWwindow;

// The following functions are declared in src/exports/Graphics.cc

SP_EXPORT sp_vk_dispatch_loader_t *sp_get_vulkan_dispatch_loader();

SP_EXPORT sp_graphics_ctx_t *sp_game_get_graphics_context(sp_game_t *ctx);

SP_EXPORT void sp_game_enable_xr_system(sp_game_t *ctx, bool enable);

SP_EXPORT void sp_graphics_set_vulkan_instance(sp_graphics_ctx_t *graphics,
    VkInstance instance,
    void (*destroy_callback)(sp_graphics_ctx_t *, VkInstance));
SP_EXPORT VkInstance sp_graphics_get_vulkan_instance(sp_graphics_ctx_t *graphics);

SP_EXPORT void sp_graphics_set_vulkan_surface(sp_graphics_ctx_t *graphics,
    VkSurfaceKHR surface,
    void (*destroy_callback)(sp_graphics_ctx_t *, VkSurfaceKHR));
SP_EXPORT VkSurfaceKHR sp_graphics_get_vulkan_surface(sp_graphics_ctx_t *graphics);

SP_EXPORT void sp_graphics_set_glfw_window(sp_graphics_ctx_t *graphics,
    GLFWwindow *window,
    void (*destroy_callback)(GLFWwindow *));
SP_EXPORT GLFWwindow *sp_graphics_get_glfw_window(sp_graphics_ctx_t *graphics);

SP_EXPORT void sp_graphics_set_winit_context(sp_graphics_ctx_t *graphics,
    sp_winit_ctx_t *window,
    void (*destroy_callback)(sp_winit_ctx_t *));
SP_EXPORT sp_winit_ctx_t *sp_graphics_get_winit_context(sp_graphics_ctx_t *graphics);

typedef struct sp_video_mode_t {
    uint32_t width;
    uint32_t height;
    // TODO: Color modes?
} sp_video_mode_t;

typedef struct sp_window_handlers_t {
    void (*get_video_modes)(sp_graphics_ctx_t *, size_t *, sp_video_mode_t *);
    void (*set_title)(sp_graphics_ctx_t *, const char *);
    bool (*should_close)(sp_graphics_ctx_t *);
    void (*update_window_view)(sp_graphics_ctx_t *, int *, int *);
    void (*set_cursor_visible)(sp_graphics_ctx_t *, bool);
    void *win32_handle;
} sp_window_handlers_t;

SP_EXPORT void sp_graphics_set_window_handlers(sp_graphics_ctx_t *graphics, const sp_window_handlers_t *handlers);
SP_EXPORT bool sp_graphics_handle_input_frame(sp_graphics_ctx_t *graphics);
SP_EXPORT void sp_graphics_step_thread(sp_graphics_ctx_t *graphics, unsigned int count);

#ifdef __cplusplus
}
#endif
