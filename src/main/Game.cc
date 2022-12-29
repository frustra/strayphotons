#include "Game.hh"

#include "assets/ConsoleScript.hh"
#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
#include "core/Tracing.hh"
#include "core/assets/AssetManager.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsContext.hh"
#endif

#ifdef SP_AUDIO_SUPPORT
    #include "audio/AudioManager.hh"
#endif

#include <atomic>
#include <cxxopts.hpp>
#include <glm/glm.hpp>

#if RUST_CXX
    #include <lib.rs.h>
#endif

namespace sp {
    Game::Game(cxxopts::ParseResult &options, const ConsoleScript *startupScript)
        : options(options), startupScript(startupScript),
#ifdef SP_GRAPHICS_SUPPORT
          graphics(this),
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
          physics(startupScript != nullptr),
#endif
#ifdef SP_XR_SUPPORT
          xr(this),
#endif
#ifdef SP_AUDIO_SUPPORT
          audio(new AudioManager),
#endif
          logic(startupScript != nullptr) {
    }

    Game::~Game() {}

    Game::ShutdownManagers::~ShutdownManagers() {
        GetConsoleManager().Shutdown();
        GetSceneManager().Shutdown();
    }

    int Game::Start() {
        tracy::SetThreadName("Main");
        int exitCode;
        std::atomic_flag exitTriggered;

        CFunc<int> cfExit("exit", "Quits the game", [&exitCode, &exitTriggered](int arg) {
            Tracef("Exit triggered via console command");
            exitCode = arg;
            exitTriggered.test_and_set();
            exitTriggered.notify_all();
        });

        Debugf("Bytes of memory used per entity: %u", ecs::World().GetBytesPerEntity());

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
        }

#if RUST_CXX
        sp::rust::print_hello();
#endif

        if (options.count("cvar")) {
            for (auto cvarline : options["cvar"].as<vector<string>>()) {
                GetConsoleManager().ParseAndExecute(cvarline);
            }
        }

#ifdef SP_GRAPHICS_SUPPORT
        if (options["headless"].count() == 0) {
            debugGui = std::make_unique<DebugGuiManager>();
            graphics.Init();

            menuGui = std::make_unique<MenuGuiManager>(this->graphics);
        }
#endif

#ifdef SP_XR_SUPPORT
        if (options["no-vr"].count() == 0) xr.LoadXrSystem();
#endif

        auto &scenes = GetSceneManager();
        scenes.QueueAction(SceneAction::ReloadPlayer);
        scenes.QueueAction(SceneAction::ReloadBindings);

        if (startupScript != nullptr) {
            funcs.Register<int>("sleep", "Pause script execution for N milliseconds", [](int ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            });
            funcs.Register("syncscene", "Pause script until all scenes are loaded", []() {
                GetSceneManager().QueueActionAndBlock(SceneAction::SyncScene);
            });

            GetConsoleManager().QueueParseAndExecute("syncscene");

            Debugf("Running script: %s", startupScript->path);
        } else {
            if (options.count("map")) {
                scenes.QueueAction(SceneAction::LoadScene, options["map"].as<string>());
            } else {
                scenes.QueueAction(SceneAction::LoadScene, "menu");
            }
        }

        GetConsoleManager().StartThread(startupScript);
        logic.StartThread();

#ifdef SP_GRAPHICS_SUPPORT
        while (!exitTriggered.test()) {
            if (!graphics.Frame()) {
                Tracef("Exit triggered via window manager");
                break;
            }
            FrameMark;
        }
#else
        while (!exitTriggered.test()) {
            exitTriggered.wait(false);
        }
#endif
        return exitCode;
    }
} // namespace sp
