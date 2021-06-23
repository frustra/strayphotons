#include "Game.hh"

#include "assets/Script.hh"
#include "core/Common.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"
#include "physx/PhysxUtils.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

#if RUST_CXX
    #include <lib.rs.h>
#endif

namespace sp {
    Game::Game(cxxopts::ParseResult &options, Script *startupScript)
        : options(options), startupScript(startupScript), graphics(this), logic(this), physics(this),
          animation(entityManager.tecs), humanControlSystem(&this->entityManager, &this->input, &this->physics)
    /*, xr(this)*/ {}

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
            graphics.Init();

            debugGui = std::make_unique<DebugGuiManager>(this->graphics, this->input);
            menuGui = std::make_unique<MenuGuiManager>(this->graphics, this->input);

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
        input.BeginFrame();
        GetConsoleManager().Update(startupScript);

        auto frameTime = chrono_clock::now();
        double dt = (double)(frameTime - lastFrameTime).count();
        dt /= chrono_clock::duration(std::chrono::seconds(1)).count();

        if (!logic.Frame(dt)) return false;
        // if (!xr.Frame(dt)) return false;
        if (!graphics.Frame()) return false;
        if (!physics.LogicFrame()) return false;
        if (!animation.Frame(dt)) return false;

        lastFrameTime = frameTime;
        return true;
    }

    bool Game::ShouldStop() {
        return !graphics.HasActiveContext();
    }
} // namespace sp
