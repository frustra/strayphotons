/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "game/CGameContext.hh"
#include "graphics/core/GraphicsManager.hh"

#include <strayphotons.h>
#include <strayphotons/export.h>
#include <vulkan/vulkan.hpp>

using namespace sp;

SP_EXPORT sp_vk_dispatch_loader_t *sp_get_vulkan_dispatch_loader() {
    return &VULKAN_HPP_DEFAULT_DISPATCHER;
}

SP_EXPORT sp_graphics_ctx_t *sp_game_get_graphics_context(sp_game_t ctx) {
    Assertf(ctx != nullptr, "sp_game_get_graphics_context called with null game ctx");
    return ctx->game.graphics.get();
}

SP_EXPORT void sp_game_enable_xr_system(sp_game_t ctx, bool enable) {
    Assertf(ctx != nullptr, "sp_game_enable_xr_system called with null game ctx");
    ctx->game.enableXrSystem = enable;
}

SP_EXPORT void sp_graphics_set_vulkan_instance(sp_graphics_ctx_t *graphics,
    VkInstance instance,
    void (*destroy_callback)(sp_graphics_ctx_t *, VkInstance)) {
    Assertf(graphics != nullptr, "sp_graphics_set_vulkan_instance called with null graphics");
    graphics->vkInstance = std::shared_ptr<VkInstance_T>(instance, [graphics, destroy_callback](VkInstance instance) {
        if (destroy_callback) destroy_callback(graphics, instance);
    });
}

SP_EXPORT VkInstance sp_graphics_get_vulkan_instance(sp_graphics_ctx_t *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_vulkan_instance called with null graphics");
    return graphics->vkInstance.get();
}

SP_EXPORT void sp_graphics_set_vulkan_surface(sp_graphics_ctx_t *graphics,
    VkSurfaceKHR surface,
    void (*destroy_callback)(sp_graphics_ctx_t *, VkSurfaceKHR)) {
    Assertf(graphics != nullptr, "sp_graphics_set_vulkan_surface called with null graphics");
    graphics->vkSurface = std::shared_ptr<VkSurfaceKHR_T>(surface, [graphics, destroy_callback](VkSurfaceKHR surface) {
        if (destroy_callback) destroy_callback(graphics, surface);
    });
}

SP_EXPORT VkSurfaceKHR sp_graphics_get_vulkan_surface(sp_graphics_ctx_t *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_vulkan_surface called with null graphics");
    return graphics->vkSurface.get();
}

SP_EXPORT void sp_graphics_set_glfw_window(sp_graphics_ctx_t *graphics,
    GLFWwindow *window,
    void (*destroy_callback)(GLFWwindow *)) {
    Assertf(graphics != nullptr, "sp_graphics_set_glfw_window called with null graphics");
    if (window) {
        graphics->glfwWindow = std::shared_ptr<GLFWwindow>(window, destroy_callback);
    } else {
        graphics->glfwWindow.reset();
    }
}

SP_EXPORT GLFWwindow *sp_graphics_get_glfw_window(sp_graphics_ctx_t *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_glfw_window called with null graphics");
    return graphics->glfwWindow.get();
}

SP_EXPORT void sp_graphics_set_winit_context(sp_graphics_ctx_t *graphics,
    sp_winit_ctx_t *ctx,
    void (*destroy_callback)(sp_winit_ctx_t *)) {
    Assertf(graphics != nullptr, "sp_graphics_set_winit_context called with null graphics");
    if (ctx) {
        graphics->winitContext = std::shared_ptr<sp::winit::WinitContext>(ctx, destroy_callback);
    } else {
        graphics->winitContext.reset();
    }
}

SP_EXPORT sp_winit_ctx_t *sp_graphics_get_winit_context(sp_graphics_ctx_t *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_winit_context called with null graphics");
    return graphics->winitContext.get();
}

SP_EXPORT void sp_graphics_set_window_handlers(sp_graphics_ctx_t *graphics, const sp_window_handlers_t *handlers) {
    Assertf(graphics != nullptr, "sp_graphics_set_window_handlers called with null graphics");
    if (handlers) {
        graphics->windowHandlers = *handlers;
    } else {
        graphics->windowHandlers = {};
    }
}

SP_EXPORT bool sp_graphics_handle_input_frame(sp_graphics_ctx_t *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_handle_input_frame called with null graphics");
    return graphics->InputFrame();
}

SP_EXPORT void sp_graphics_step_thread(sp_graphics_ctx_t *graphics, unsigned int count) {
    Assertf(graphics != nullptr, "sp_graphics_step_thread called with null graphics");
    graphics->Step(count);
}
