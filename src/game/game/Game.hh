/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CFunc.hh"
#include "console/ConsoleBindingManager.hh"
#include "core/Common.hh"
#include "core/Defer.hh"
#include "core/LockFreeEventQueue.hh"
#include "ecs/Ecs.hh"
#include "editor/EditorSystem.hh"
#include "game/GameLogic.hh"

#include <chrono>
#include <memory>
#include <vector>

namespace cxxopts {
    class ParseResult;
}

namespace sp::xr {
    class XrManager;
}

namespace sp {
    class ConsoleScript;
    struct CGameContext;

    class GraphicsManager;
    class PhysxManager;
    class AudioManager;

    class Game {
        LogOnExit logOnExit = "Game shut down ========================================================";

    public:
        Game(CGameContext &ctx);
        ~Game();

        int Start();

        CGameContext &gameContext;
        cxxopts::ParseResult &options;
        CFuncCollection funcs;

        std::atomic_int exitCode;
        std::atomic_flag exitTriggered;

    private:
        // Shutdown managers before CFuncs are destroyed above.
        struct ShutdownManagers {
            ~ShutdownManagers();
        } shutdownManagers;

    public:
        LockFreeEventQueue<ecs::Event> inputEventQueue;

        std::atomic_uint64_t graphicsStepCount, graphicsMaxStepCount;
        std::shared_ptr<GraphicsManager> graphics;
        void (*shutdownCallback)(CGameContext *) = nullptr;

        std::shared_ptr<PhysxManager> physics;
        std::shared_ptr<xr::XrManager> xr;
        std::shared_ptr<AudioManager> audio;

        ConsoleBindingManager consoleBinding;
        EditorSystem editor;
        GameLogic logic;
    };

    void RegisterDebugCFuncs();
} // namespace sp
