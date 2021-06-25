#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"
#include "game/GameLogic.hh"
#include "game/systems/AnimationSystem.hh"
#include "graphics/GraphicsManager.hh"
#include "input/InputManager.hh"
#include "physx/PhysxManager.hh"
// #include "xr/XrManager.hh"

#ifdef SP_GRAPHICS_SUPPORT_GL
    #include "graphics/opengl/gui/DebugGuiManager.hh"
    #include "graphics/opengl/gui/MenuGuiManager.hh"
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

        GraphicsManager graphics;
        InputManager input;
        GameLogic logic;
        PhysxManager physics;
        AnimationSystem animation;
        HumanControlSystem humanControlSystem;
        // xr::XrManager xr;

    private:
        chrono_clock::time_point lastFrameTime;
    };
} // namespace sp
