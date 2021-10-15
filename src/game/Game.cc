#include "Game.hh"

#include "assets/Script.hh"
#include "core/Common.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
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

#ifdef SP_INPUT_SUPPORT
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
        }
#endif

#if RUST_CXX
        sp::rust::print_hello();
#endif

        try {
#ifdef SP_GRAPHICS_SUPPORT
            debugGui = std::make_unique<DebugGuiManager>();
            menuGui = std::make_unique<MenuGuiManager>(this->graphics);

            graphics.Init();
#endif

            logic.LoadPlayer();

#ifdef SP_XR_SUPPORT
            if (options["no-vr"].count() == 0) xr.LoadXrSystem();
#endif

#ifdef SP_INPUT_SUPPORT
            inputBindingLoader.Load(InputBindingConfigPath);
#endif

            if (options.count("map")) { logic.LoadScene(options["map"].as<string>()); }

            if (startupScript != nullptr) {
                startupScript->Exec();
            } else if (!options.count("map")) {
                logic.LoadScene("menu");
#ifdef SP_INPUT_SUPPORT
                {
                    auto lock = ecs::World.StartTransaction<ecs::Write<ecs::FocusLock>>();
                    lock.Get<ecs::FocusLock>().AcquireFocus(ecs::FocusLayer::MENU);
                }
#endif
            }

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
        GetConsoleManager().Update(startupScript);

        auto frameTime = chrono_clock::now();
        double dt = (double)(frameTime - lastFrameTime).count();
        dt /= chrono_clock::duration(std::chrono::seconds(1)).count();

#ifdef SP_INPUT_SUPPORT_GLFW
        if (glfwInputHandler) glfwInputHandler->Frame();
#endif
        if (!logic.Frame(dt)) return false;
#ifdef SP_GRAPHICS_SUPPORT
        if (!graphics.Frame()) return false;
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
