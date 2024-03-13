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

using namespace sp;

SP_EXPORT GraphicsManager *sp_game_get_graphics_manager(sp_game_t ctx) {
    Assertf(ctx != nullptr, "sp_game_get_graphics_manager called with null ctx");
    return ctx->game.graphics.get();
}

SP_EXPORT void sp_graphics_set_vulkan_instance(GraphicsManager *graphics,
    VkInstance instance,
    void (*destroy_callback)(GraphicsManager *, VkInstance)) {
    Assertf(graphics != nullptr, "sp_graphics_set_vulkan_instance called with null graphics");
    graphics->vkInstance = std::shared_ptr<VkInstance_T>(instance, [graphics, destroy_callback](VkInstance instance) {
        if (destroy_callback) destroy_callback(graphics, instance);
    });
}

SP_EXPORT VkInstance sp_graphics_get_vulkan_instance(GraphicsManager *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_vulkan_instance called with null graphics");
    return graphics->vkInstance.get();
}

SP_EXPORT void sp_graphics_set_vulkan_surface(GraphicsManager *graphics,
    VkSurfaceKHR surface,
    void (*destroy_callback)(GraphicsManager *, VkSurfaceKHR)) {
    Assertf(graphics != nullptr, "sp_graphics_set_vulkan_surface called with null graphics");
    graphics->vkSurface = std::shared_ptr<VkSurfaceKHR_T>(surface, [graphics, destroy_callback](VkSurfaceKHR surface) {
        if (destroy_callback) destroy_callback(graphics, surface);
    });
}

SP_EXPORT VkSurfaceKHR sp_graphics_get_vulkan_surface(GraphicsManager *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_vulkan_surface called with null graphics");
    return graphics->vkSurface.get();
}

SP_EXPORT void sp_graphics_set_glfw_window(GraphicsManager *graphics,
    GLFWwindow *window,
    void (*destroy_callback)(GLFWwindow *)) {
    Assertf(graphics != nullptr, "sp_graphics_set_glfw_window called with null graphics");
    if (window) {
        graphics->glfwWindow = std::shared_ptr<GLFWwindow>(window, destroy_callback);
    } else {
        graphics->glfwWindow.reset();
    }
}

SP_EXPORT GLFWwindow *sp_graphics_get_glfw_window(GraphicsManager *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_glfw_window called with null graphics");
    return graphics->glfwWindow.get();
}

SP_EXPORT void sp_graphics_set_winit_context(GraphicsManager *graphics,
    sp_winit_ctx_t *ctx,
    void (*destroy_callback)(sp_winit_ctx_t *)) {
    Assertf(graphics != nullptr, "sp_graphics_set_winit_context called with null graphics");
    if (ctx) {
        graphics->winitContext = std::shared_ptr<sp::winit::WinitContext>(ctx, destroy_callback);
    } else {
        graphics->winitContext.reset();
    }
}

SP_EXPORT sp_winit_ctx_t *sp_graphics_get_winit_context(GraphicsManager *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_get_winit_context called with null graphics");
    return graphics->winitContext.get();
}

SP_EXPORT void sp_graphics_set_window_handlers(GraphicsManager *graphics, const sp_window_handlers_t *handlers) {
    Assertf(graphics != nullptr, "sp_graphics_set_window_handlers called with null graphics");
    graphics->windowHandlers = *handlers;
}

SP_EXPORT bool sp_graphics_handle_input_frame(GraphicsManager *graphics) {
    Assertf(graphics != nullptr, "sp_graphics_handle_input_frame called with null graphics");
    return graphics->InputFrame();
}
