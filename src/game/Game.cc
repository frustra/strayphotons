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

#include <cxxopts.hpp>
#include <glm/glm.hpp>

#if RUST_CXX
    #include <lib.rs.h>
#endif

namespace sp {
    static sp::CVar<std::string> CVarFlatviewEntity("r.FlatviewEntity",
        "player.flatview",
        "The entity with a View component to display");

    Game::Game(cxxopts::ParseResult &options, Script *startupScript)
        : options(options), startupScript(startupScript), logic(startupScript != nullptr)
#ifdef SP_GRAPHICS_SUPPORT
          ,
          graphics(this)
#endif
#ifdef SP_XR_SUPPORT
          ,
          xr(this)
#endif
    {
        funcs.Register(this, "reloadplayer", "Reload player scene", &Game::ReloadPlayer);
        funcs.Register(this, "printdebug", "Print some debug info about the scene", &Game::PrintDebug);
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

        Debugf("Bytes of memory used per entity: %u", ecs::World.GetBytesPerEntity());

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
        }

#if RUST_CXX
        sp::rust::print_hello();
#endif

        try {
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
            ReloadPlayer();
            if (options.count("map")) {
                scenes.QueueActionAndBlock(SceneAction::LoadScene, options["map"].as<string>());
            }

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

            while (!triggeredExit) {
#ifdef SP_INPUT_SUPPORT_GLFW
                if (glfwInputHandler) glfwInputHandler->Frame();
#endif
#ifdef SP_GRAPHICS_SUPPORT
                if (!graphics.Frame()) break;
                FrameMark;
#else
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif
            }
            return exitCode;
        } catch (char const *err) {
            Errorf("%s", err);
            return 1;
        }
    }

    void Game::ReloadPlayer() {
        auto &scenes = GetSceneManager();
        scenes.QueueActionAndBlock(SceneAction::LoadPlayer);

        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>>();
            player = ecs::EntityWith<ecs::Name>(lock, "player.player");
            flatview = ecs::EntityWith<ecs::Name>(lock, CVarFlatviewEntity.Get());
        }

#ifdef SP_GRAPHICS_SUPPORT
        auto context = graphics.GetContext();
        if (context) context->AttachView(flatview);
#endif

        scenes.RespawnPlayer(player);
        scenes.QueueActionAndBlock(SceneAction::LoadBindings);
    }

    void Game::PrintDebug() {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::TransformSnapshot, ecs::CharacterController, ecs::LightSensor>>();
        if (flatview && flatview.Has<ecs::TransformSnapshot>(lock)) {
            auto &transform = flatview.Get<ecs::TransformSnapshot>(lock);
            auto position = transform.GetPosition();
            Logf("Flatview position: [%f, %f, %f]", position.x, position.y, position.z);
        }
        if (player && player.Has<ecs::TransformSnapshot>(lock)) {
            auto &transform = player.Get<ecs::TransformSnapshot>(lock);
            auto position = transform.GetPosition();
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            if (player.Has<ecs::CharacterController>(lock)) {
                auto &controller = player.Get<ecs::CharacterController>(lock);
                if (controller.pxController) {
                    auto pxFeet = controller.pxController->getFootPosition();
                    Logf("Player physics position: [%f, %f, %f]", pxFeet.x, pxFeet.y, pxFeet.z);
                    auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
                    Logf("Player velocity: [%f, %f, %f]",
                        userData->actorData.velocity.x,
                        userData->actorData.velocity.y,
                        userData->actorData.velocity.z);
                    Logf("Player on ground: %s", userData->onGround ? "true" : "false");
                } else {
                    Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
                }
            } else {
                Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
            }
#else
            Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
#endif
        } else {
            Logf("Scene has no valid player");
        }

        for (auto ent : lock.EntitiesWith<ecs::LightSensor>()) {
            auto &sensor = ent.Get<ecs::LightSensor>(lock);
            auto &i = sensor.illuminance;

            Logf("Light sensor %s: %f %f %f", ecs::ToString(lock, ent), i.r, i.g, i.b);
        }
    }
} // namespace sp
