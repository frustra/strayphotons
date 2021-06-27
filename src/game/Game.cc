#include "Game.hh"

#include "assets/Script.hh"
#include "core/Common.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

#if RUST_CXX
    #include <lib.rs.h>
#endif

namespace sp {
    Game::Game(cxxopts::ParseResult &options, Script *startupScript)
        : options(options), startupScript(startupScript),
#ifdef SP_GRAPHICS_SUPPORT
          graphics(this),
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
          physics(entityManager.tecs),
          humanControlSystem(entityManager.tecs, &this->input, &this->physics),
#endif
          animation(entityManager.tecs),
#ifdef SP_XR_SUPPORT
          xr(this),
#endif
          logic(this) {
    }

    Game::~Game() {}

    int Game::Start() {
        bool triggeredExit = false;
        int exitCode = 0;

        CFunc<int> cfExit("exit", "Quits the game", [&](int arg) {
            triggeredExit = true;
            exitCode = arg;
        });

        if (options.count("cvar")) {
            for (auto cvarline : options["cvar"].as<vector<string>>()) {
                GetConsoleManager().ParseAndExecute(cvarline);
            }
        }

#if RUST_CXX
        sp::rust::print_hello();
#endif

        try {
#ifdef SP_GRAPHICS_SUPPORT
#ifdef SP_GRAPHICS_SUPPORT_GL
            debugGui = std::make_unique<DebugGuiManager>(this->graphics, this->input);
            menuGui = std::make_unique<MenuGuiManager>(this->graphics, this->input);
#endif
            graphics.Init();
#endif

            logic.Init(startupScript);

            lastFrameTime = chrono_clock::now();

            while (!triggeredExit) {
                if (ShouldStop()) break;
                if (!Frame()) break;
            }
            return exitCode;
        } catch (char const *err) {
            Errorf("%s", err);
            return 1;
        }
    }

    bool Game::Frame() {
#ifdef SP_INPUT_SUPPORT
        input.BeginFrame();
#endif

        GetConsoleManager().Update(startupScript);

        auto frameTime = chrono_clock::now();
        double dt = (double)(frameTime - lastFrameTime).count();
        dt /= chrono_clock::duration(std::chrono::seconds(1)).count();

        if (!logic.Frame(dt)) return false;
#ifdef SP_XR_SUPPORT
        if (!xr.Frame(dt)) return false;
#endif
#ifdef SP_GRAPHICS_SUPPORT
        if (!graphics.Frame()) return false;
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
        if (!humanControlSystem.Frame(dt)) return false;
        if (!physics.LogicFrame()) return false;
#endif
        if (!animation.Frame(dt)) return false;

        lastFrameTime = frameTime;
        return true;
    }

    bool Game::ShouldStop() {
#ifdef SP_GRAPHICS_SUPPORT
        return !graphics.HasActiveContext();
#else
        return false;
#endif
    }
} // namespace sp
