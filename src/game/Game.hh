#pragma once

#include "core/Common.hh"
#include "core/input/InputManager.hh"
#include "ecs/Ecs.hh"
#include "game/GameLogic.hh"
#include "game/gui/DebugGuiManager.hh"
#include "game/gui/MenuGuiManager.hh"
#include "game/systems/AnimationSystem.hh"
#include "graphics/GraphicsManager.hh"
#include "physx/PhysxManager.hh"
// #include "xr/XrManager.hh"

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

        std::unique_ptr<DebugGuiManager> debugGui = nullptr;
        std::unique_ptr<MenuGuiManager> menuGui = nullptr;

        GraphicsManager graphics;
        InputManager input;
        ecs::EntityManager entityManager;
        GameLogic logic;
        PhysxManager physics;
        AnimationSystem animation;
        HumanControlSystem humanControlSystem;
        // xr::XrManager xr;

    private:
        chrono_clock::time_point lastFrameTime;
    };
} // namespace sp
