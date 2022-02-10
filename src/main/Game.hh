#pragma once

#include "console/ConsoleBindingManager.hh"
#include "core/Common.hh"
#include "ecs/Ecs.hh"
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
    class Script;

#ifdef SP_AUDIO_SUPPORT
    class AudioManager;
#endif

    class Game {
    public:
        Game(cxxopts::ParseResult &options, Script *startupScript = nullptr);
        ~Game();

        int Start();

        cxxopts::ParseResult &options;
        Script *startupScript = nullptr;

#ifdef SP_GRAPHICS_SUPPORT
        GraphicsManager graphics;

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
        GameLogic logic;
    };
} // namespace sp
