#include "Game.hh"

#include "assets/Script.hh"
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

#include <atomic>
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
          logic(startupScript != nullptr) {
    }

    Game::~Game() {}

    int Game::Start() {
        int exitCode;
        std::atomic_flag exitTriggered;

        CFunc<int> cfExit("exit", "Quits the game", [&exitCode, &exitTriggered](int arg) {
            exitCode = arg;
            exitTriggered.test_and_set();
            exitTriggered.notify_all();
        });

        if (options.count("cvar")) {
            for (auto cvarline : options["cvar"].as<vector<string>>()) {
                GetConsoleManager().ParseAndExecute(cvarline);
            }
        }

        Debugf("Bytes of memory used per entity: %u", ecs::World.GetBytesPerEntity());

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
        }

#if RUST_CXX
        sp::rust::print_hello();
#endif

#ifdef SP_GRAPHICS_SUPPORT
        if (options["headless"].count() == 0) {
            debugGui = std::make_unique<DebugGuiManager>();
            graphics.Init();

            // must create all gui instances after initializing graphics, except for the special debug gui
            menuGui = std::make_unique<MenuGuiManager>(this->graphics);
        }
#endif

#ifdef SP_XR_SUPPORT
        if (options["no-vr"].count() == 0) xr.LoadXrSystem();
#endif

        auto &scenes = GetSceneManager();
        scenes.QueueActionAndBlock(SceneAction::ReloadPlayer);
        scenes.QueueActionAndBlock(SceneAction::ReloadBindings);
        if (options.count("map")) { scenes.QueueActionAndBlock(SceneAction::LoadScene, options["map"].as<string>()); }

        if (startupScript != nullptr) {
            startupScript->Exec();
        } else if (!options.count("map")) {
            scenes.QueueActionAndBlock(SceneAction::LoadScene, "menu");
            {
                auto lock = ecs::World.StartTransaction<ecs::Write<ecs::FocusLock>>();
                lock.Get<ecs::FocusLock>().AcquireFocus(ecs::FocusLayer::MENU);
            }
        }

        logic.StartThread();

#ifdef SP_GRAPHICS_SUPPORT
        while (!exitTriggered.test()) {
            if (!graphics.Frame()) break;
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
