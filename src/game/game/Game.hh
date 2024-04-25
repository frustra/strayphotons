/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/Defer.hh"
#include "common/LockFreeEventQueue.hh"
#include "console/CFunc.hh"
#include "console/ConsoleBindingManager.hh"
#include "ecs/Ecs.hh"
#include "editor/EditorSystem.hh"
#include "game/GameLogic.hh"

#include <chrono>
#include <memory>
#include <strayphotons.h>
#include <vector>

namespace cxxopts {
    class ParseResult;
}

namespace sp::xr {
    class XrSystem;
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

        std::shared_ptr<GraphicsManager> graphics;
        std::shared_ptr<PhysxManager> physics;
        std::shared_ptr<AudioManager> audio;
        std::shared_ptr<xr::XrSystem> xr;
        bool enableXrSystem = false;

        ConsoleBindingManager consoleBinding;
        EditorSystem editor;
        GameLogic logic;
    };

    void RegisterDebugCFuncs(CFuncCollection &funcs);
    void InitAudioManager(Game &game);
    void InitGraphicsManager(Game &game);
    void StartGraphicsThread(Game &game, bool scriptMode);
    void InitPhysicsManager(Game &game);
    void StartPhysicsThread(Game &game, bool scriptMode);
    void LoadXrSystem(Game &game);
    void InitRust(Game &game);
} // namespace sp
