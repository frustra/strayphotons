#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"
#include "game/GameLogic.hh"
#include "systems/AnimationSystem.hh"

#ifdef SP_INPUT_SUPPORT
    #include "input/InputManager.hh"
#endif

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/GraphicsManager.hh"
#endif

#ifdef SP_GRAPHICS_SUPPORT_GL
    #include "graphics/opengl/gui/DebugGuiManager.hh"
    #include "graphics/opengl/gui/MenuGuiManager.hh"
#endif

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    // #include "physx/HumanControlSystem.hh"
    #include "physx/PhysxManager.hh"
#endif

#ifdef SP_XR_SUPPORT
    #include "xr/XrManager.hh"
#endif

#include <chrono>

namespace cxxopts {
    class ParseResult;
}

namespace sp {
    class Script;

    class Game {
    public:
        Game(cxxopts::ParseResult &options, Script *startupScript = nullptr);
        ~Game();

        int Start();
        bool Frame();
        void PhysicsUpdate();
        bool ShouldStop();

        cxxopts::ParseResult &options;
        Script *startupScript = nullptr;

        ecs::EntityManager entityManager;

#ifdef SP_GRAPHICS_SUPPORT_GL
        std::unique_ptr<DebugGuiManager> debugGui = nullptr;
        std::unique_ptr<MenuGuiManager> menuGui = nullptr;
#endif

#ifdef SP_GRAPHICS_SUPPORT
        GraphicsManager graphics;
#endif
#ifdef SP_INPUT_SUPPORT
        InputManager input;
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
        PhysxManager physics;
        // HumanControlSystem humanControlSystem;
#endif
        AnimationSystem animation;
#ifdef SP_XR_SUPPORT
        xr::XrManager xr;
#endif
        GameLogic logic;

    private:
        chrono_clock::time_point lastFrameTime;
    };
} // namespace sp
