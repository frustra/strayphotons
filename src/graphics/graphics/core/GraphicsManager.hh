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
#include <vector>

namespace sp::vulkan {
    class Renderer;
}
namespace cxxopts {
    class ParseResult;
}

namespace sp {
    class Game;
    class GraphicsContext;
    class DebugGuiManager;
    class MenuGuiManager;

    class GraphicsManager : public RegisteredThread {
        LogOnExit logOnExit = "Graphics shut down ====================================================";

    public:
        GraphicsManager(cxxopts::ParseResult &options);
        ~GraphicsManager();

        operator bool() const {
            return initialized;
        }

        void Init();
        void StartThread(bool startPaused = false);
        void StopThread();
        bool HasActiveContext();
        bool InputFrame();

        std::shared_ptr<GraphicsContext> context;

    private:
        bool ThreadInit() override;
        void PreFrame() override;
        void PostFrame(bool stepMode) override;
        void Frame() override;

        cxxopts::ParseResult &options;
        ecs::ECS &world;
        ecs::Name flatviewName;

        chrono_clock::time_point renderStart;

        std::shared_ptr<DebugGuiManager> debugGui;
        std::shared_ptr<MenuGuiManager> menuGui;

        bool initialized = false;
    };
} // namespace sp
