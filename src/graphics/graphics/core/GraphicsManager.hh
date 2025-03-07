/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <memory>
#include <strayphotons.h>
#include <vector>

struct GLFWwindow;
struct VkInstance_T;
struct VkSurfaceKHR_T;

namespace sp::vulkan {
    class Renderer;
}

namespace sp::winit {
    struct WinitContext;
}

namespace sp {
    class Game;
    class GraphicsContext;
    class DebugGuiManager;
    class MenuGuiManager;

    class GraphicsManager : public RegisteredThread {
        LogOnExit logOnExit = "Graphics shut down ====================================================";

    public:
        GraphicsManager(Game &game);
        ~GraphicsManager();

        operator bool() const {
            return initialized;
        }

        void Init();
        void StartThread(bool startPaused = false);
        void StopThread();
        bool HasActiveContext();
        bool InputFrame();

        sp_window_handlers_t windowHandlers = {0};

        // Note: deconstruction order for the below fields is important.
        std::shared_ptr<VkInstance_T> vkInstance;
        std::shared_ptr<VkSurfaceKHR_T> vkSurface;

        std::shared_ptr<GLFWwindow> glfwWindow;
        std::shared_ptr<sp::winit::WinitContext> winitContext;

        std::shared_ptr<GraphicsContext> context;

    private:
        bool ThreadInit() override;
        bool PreFrame() override;
        void PostFrame(bool stepMode) override;
        void Frame() override;

        Game &game;
        ecs::Name flatviewName;

        chrono_clock::time_point renderStart;

        std::shared_ptr<DebugGuiManager> debugGui;
        std::shared_ptr<MenuGuiManager> menuGui;

        bool initialized = false;
    };
} // namespace sp
