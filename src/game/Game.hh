#pragma once

#include "console/ConsoleBindingManager.hh"
#include "core/Common.hh"
#include "ecs/Ecs.hh"
#include "game/AnimationSystem.hh"

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/GraphicsManager.hh"
    #include "graphics/gui/DebugGuiManager.hh"
    #include "graphics/gui/MenuGuiManager.hh"
#endif

#ifdef SP_INPUT_SUPPORT_GLFW
    #include "input/glfw/GlfwInputHandler.hh"
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

    class Game {
    public:
        Game(cxxopts::ParseResult &options, Script *startupScript = nullptr);
        ~Game();

        int Start();
        bool Frame();
        void PhysicsUpdate();
        bool ShouldStop();

        void ReloadPlayer();

        void PrintDebug();
        void PrintEvents(std::string entityName);
        void PrintSignals(std::string entityName);

        cxxopts::ParseResult &options;
        Script *startupScript = nullptr;

#ifdef SP_GRAPHICS_SUPPORT
        GraphicsManager graphics;

        std::unique_ptr<DebugGuiManager> debugGui = nullptr;
        std::unique_ptr<MenuGuiManager> menuGui = nullptr;
#endif
#ifdef SP_INPUT_SUPPORT_GLFW
        std::unique_ptr<GlfwInputHandler> glfwInputHandler;
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
        PhysxManager physics;
#endif
        AnimationSystem animation;
#ifdef SP_XR_SUPPORT
        xr::XrManager xr;
#endif
        ConsoleBindingManager consoleBinding;

    private:
        chrono_clock::time_point lastFrameTime;
        CFuncCollection funcs;
    };
} // namespace sp
