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

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/GraphicsManager.hh"
    #include "graphics/gui/DebugGuiManager.hh"
    #include "graphics/gui/MenuGuiManager.hh"
#endif

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    #include "physx/PhysxManager.hh"
#endif

#ifdef SP_XR_SUPPORT
    #include "xr/XrManager.hh"
#endif

#include <chrono>
#include <memory>
#include <vector>

namespace cxxopts {
    class ParseResult;
}

namespace sp {
    class ConsoleScript;

#ifdef SP_AUDIO_SUPPORT
    class AudioManager;
#endif

    extern std::atomic_int gameExitCode;
    extern std::atomic_flag gameExitTriggered;

    class Game {
        LogOnExit logOnExit = "Game shut down ========================================================";

    public:
        Game(cxxopts::ParseResult &options, const ConsoleScript *startupScript = nullptr);
        ~Game();

        int Start();

        cxxopts::ParseResult &options;
        const ConsoleScript *startupScript = nullptr;

        CFuncCollection funcs;

    private:
        struct ShutdownManagers {
            ~ShutdownManagers();
        } shutdownManagers;

    public:
#ifdef SP_GRAPHICS_SUPPORT
        std::atomic_uint64_t graphicsStepCount, graphicsMaxStepCount;
        GraphicsManager graphics;

        LockFreeEventQueue<ecs::Event> windowEventQueue;

        std::unique_ptr<DebugGuiManager> debugGui = nullptr;
        std::unique_ptr<MenuGuiManager> menuGui = nullptr;
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
        PhysxManager physics;
#endif
#ifdef SP_XR_SUPPORT
        xr::XrManager xr;
#endif
#ifdef SP_AUDIO_SUPPORT
        std::unique_ptr<AudioManager> audio;
#endif
        ConsoleBindingManager consoleBinding;
        EditorSystem editor;
        GameLogic logic;
    };
} // namespace sp
