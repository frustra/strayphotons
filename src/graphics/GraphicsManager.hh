/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#ifdef SP_GRAPHICS_SUPPORT

    #include "core/Common.hh"
    #include "core/RegisteredThread.hh"
    #include "ecs/Ecs.hh"
    #include "ecs/EntityRef.hh"

    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/core/VkCommon.hh"
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        #include "input/glfw/GlfwInputHandler.hh"
    #endif

    #ifdef SP_INPUT_SUPPORT_WINIT
        #include "input/winit/WinitInputHandler.hh"
    #endif

    #include <glm/glm.hpp>
    #include <memory>
    #include <vector>

namespace sp {
    class Game;
    class GraphicsContext;
    class ProfilerGui;

    #ifdef SP_GRAPHICS_SUPPORT_VK
    namespace vulkan {
        class Renderer;
        class GuiRenderer;
        class ImageView;
        class ProfilerGui;
    } // namespace vulkan
    #endif

    class GraphicsManager : public RegisteredThread {
        LogOnExit logOnExit = "Graphics shut down ====================================================";

    public:
        GraphicsManager(Game &game);
        ~GraphicsManager();

        void Init();
        void StartThread(bool startPaused = false);
        void StopThread();
        bool HasActiveContext();
        bool InputFrame();

        GraphicsContext *GetContext();

    #ifdef SP_INPUT_SUPPORT_WINIT
        WinitInputHandler *GetWinitInputHandler() {
            return winitInputHandler.get();
        }
    #endif

    private:
        bool ThreadInit() override;
        void PreFrame() override;
        void PostFrame(bool stepMode) override;
        void Frame() override;

        Game &game;
        unique_ptr<GraphicsContext> context;
        ecs::EntityRef flatviewEntity;

        chrono_clock::time_point renderStart;

    #ifdef SP_GRAPHICS_SUPPORT_VK
        unique_ptr<vulkan::Renderer> renderer;
        bool enableSwapchain = false;
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        unique_ptr<GlfwInputHandler> glfwInputHandler;
    #endif

    #ifdef SP_INPUT_SUPPORT_WINIT
        unique_ptr<WinitInputHandler> winitInputHandler;
    #endif
    };
} // namespace sp

#endif
