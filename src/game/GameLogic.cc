#include "GameLogic.hh"

#include "Game.hh"
#include "assets/AssetManager.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/Signals.hh"

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsContext.hh"
#endif

#include <cmath>
#include <cxxopts.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

namespace sp {
    GameLogic::GameLogic(Game *game) : game(game) {
        funcs.Register(this, "loadscene", "Load a scene", &GameLogic::LoadScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &GameLogic::ReloadScene);
        funcs.Register(this, "printdebug", "Print some debug info about the scene", &GameLogic::PrintDebug);
        funcs.Register(this,
                       "setsignal",
                       "Set a signal value (setsignal <entity>.<signal> <value>)",
                       &GameLogic::SetSignal);
        funcs.Register(this,
                       "clearsignal",
                       "Clear a signal value (clearsignal <entity>.<signal>)",
                       &GameLogic::ClearSignal);
    }

    static CVar<float> CVarFlashlight("r.Flashlight", 100, "Flashlight intensity");
    static CVar<bool> CVarFlashlightOn("r.FlashlightOn", false, "Flashlight on/off");
    static CVar<string> CVarFlashlightParent("r.FlashlightParent", "player", "Flashlight parent entity name");
    static CVar<float> CVarFlashlightAngle("r.FlashlightAngle", 20, "Flashlight spot angle");
    static CVar<int> CVarFlashlightResolution("r.FlashlightResolution", 512, "Flashlight shadow map resolution");

    void GameLogic::Init(Script *startupScript) {
        LoadPlayer();

        if (game->options.count("map")) { LoadScene(game->options["map"].as<string>()); }

        if (startupScript != nullptr) {
            startupScript->Exec();
        } else if (!game->options.count("map")) {
            LoadScene("menu");
        }

#ifdef SP_INPUT_SUPPORT
        game->input.BindCommand(INPUT_ACTION_SET_VR_ORIGIN, "setvrorigin");
        game->input.BindCommand(INPUT_ACTION_RELOAD_SCENE, "reloadscene");
        game->input.BindCommand(INPUT_ACTION_RESET_SCENE, "reloadscene reset");
        game->input.BindCommand(INPUT_ACTION_RELOAD_SHADERS, "reloadshaders");
        game->input.BindCommand(INPUT_ACTION_TOGGLE_FLASHLIGH, "toggle r.FlashlightOn");
#endif
    }

    GameLogic::~GameLogic() {}

#ifdef SP_INPUT_SUPPORT
    void GameLogic::HandleInput() {
        if (game->input.FocusLocked()) return;

    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (game->menuGui && game->input.IsPressed(INPUT_ACTION_OPEN_MENU)) {
            game->menuGui->OpenPauseMenu();
        } else
    #endif
            if (game->input.IsPressed(INPUT_ACTION_SPAWN_DEBUG)) {
            // Spawn dodecahedron
            auto lock = game->entityManager.tecs.StartTransaction<ecs::AddRemove>();
            auto entity = lock.NewEntity();
            entity.Set<ecs::Owner>(lock, ecs::Owner::SystemId::GAME_LOGIC);
            auto model = GAssets.LoadModel("dodecahedron");
            entity.Set<ecs::Renderable>(lock, model);
            entity.Set<ecs::Transform>(lock, glm::vec3(0, 5, 0));

    #ifdef SP_PHYSICS_SUPPORT_PHYSX
            entity.Set<ecs::Physics>(lock, model, true);
    #endif
        } else if (game->input.IsPressed(INPUT_ACTION_DROP_FLASHLIGH)) {
            // Toggle flashlight following player

            auto lock = game->entityManager.tecs.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();
            if (flashlight && flashlight.Has<ecs::Transform>(lock)) {
                auto &transform = flashlight.Get<ecs::Transform>(lock);

                auto player = ecs::EntityWith<ecs::Name>(lock, CVarFlashlightParent.Get());
                if (player && player.Has<ecs::Transform>(lock)) {
                    auto &playerTransform = player.Get<ecs::Transform>(lock);

                    if (transform.HasParent(lock)) {
                        transform.SetPosition(transform.GetGlobalTransform(lock) * glm::vec4(0, 0, 0, 1));
                        transform.SetRotate(playerTransform.GetGlobalRotation(lock));
                        transform.SetParent(Tecs::Entity());
                    } else {
                        transform.SetPosition(glm::vec3(0, -0.3, 0));
                        transform.SetRotate(glm::quat());
                        transform.SetParent(player);
                    }
                    transform.UpdateCachedTransform(lock);
                }
            }
        }
    }
#endif

    bool GameLogic::Frame(double dtSinceLastFrame) {
#ifdef SP_INPUT_SUPPORT
        HandleInput();
#endif

        if (!scene) return true;

        for (auto entity : game->entityManager.EntitiesWith<ecs::Script>()) {
            auto script = entity.Get<ecs::Script>();
            script->OnTick(dtSinceLastFrame, game->entityManager.tecs);
        }

        for (auto entity : game->entityManager.EntitiesWith<ecs::TriggerArea>()) {
            auto area = entity.Get<ecs::TriggerArea>();

            for (auto triggerableEntity : game->entityManager.EntitiesWith<ecs::Triggerable>()) {
                auto transform = triggerableEntity.Get<ecs::Transform>();
                auto entityPos = transform->GetPosition();
                if (entityPos.x > area->boundsMin.x && entityPos.y > area->boundsMin.y &&
                    entityPos.z > area->boundsMin.z && entityPos.x < area->boundsMax.x &&
                    entityPos.y < area->boundsMax.y && entityPos.z < area->boundsMax.z && !area->triggered) {
                    area->triggered = true;
                    Debugf("Entity at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
                    Logf("Triggering event: %s", area->command);
                    GetConsoleManager().QueueParseAndExecute(area->command);
                }
            }
        }

        if (!scene) return true;

        if (CVarFlashlight.Changed()) {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::Write<ecs::Light>>();
            flashlight.Get<ecs::Light>(lock).intensity = CVarFlashlight.Get(true);
        }
        if (CVarFlashlightOn.Changed()) {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::Write<ecs::Light>>();
            flashlight.Get<ecs::Light>(lock).on = CVarFlashlightOn.Get(true);
        }
        if (CVarFlashlightAngle.Changed()) {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::Write<ecs::Light>>();
            flashlight.Get<ecs::Light>(lock).spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
        }
        if (CVarFlashlightResolution.Changed()) {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::Write<ecs::View>>();
            flashlight.Get<ecs::View>(lock).extents = glm::ivec2(CVarFlashlightResolution.Get(true));
        }

        return true;
    }

    void GameLogic::LoadPlayer() {
        game->entityManager.DestroyAllWith<ecs::Owner>(ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));

        if (playerScene != nullptr) {
            for (auto &line : playerScene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        playerScene.reset();
        player = Tecs::Entity();

        {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::AddRemove>();
            playerScene = GAssets.LoadScene("player", lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                if (e.Has<ecs::Owner>(lock)) {
                    auto &name = e.Get<ecs::Name>(lock);
                    auto &owner = e.Get<ecs::Owner>(lock);
                    if (owner == ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0)) {
                        if (name == "player") {
                            player = e;
                        } else if (name == "flashlight") {
                            flashlight = e;
                        }
                    }
                }
            }
            Assert(!!player, "Player scene doesn't contain an entity named player");

            if (!player.Has<ecs::Transform>(lock)) { player.Set<ecs::Transform>(lock, glm::vec3(0)); }
            player.Set<ecs::HumanController>(lock);
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            auto &interact = player.Set<ecs::InteractController>(lock);
            interact.manager = &game->physics;
#endif

            // Mark the player as being able to activate trigger areas
            player.Set<ecs::Triggerable>(lock);

#ifdef SP_GRAPHICS_SUPPORT
            game->graphics.GetContext()->AttachView(player);
#endif

            if (playerScene) {
                for (auto &line : playerScene->autoExecList) {
                    GetConsoleManager().ParseAndExecute(line);
                }
            }
        }
    }

    void GameLogic::LoadScene(string name) {
#ifdef SP_GRAPHICS_SUPPORT
        game->graphics.RenderLoading();
#endif
        game->entityManager.DestroyAllWith<ecs::Owner>(ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));

        if (scene != nullptr) {
            for (auto &line : scene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        scene.reset();
        {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::AddRemove>();
            scene = GAssets.LoadScene(name, lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                auto &name = e.Get<ecs::Name>(lock);
                if (name == "player" && e != player) {
                    name = "player-spawn";
                    if (e.Has<ecs::Transform>(lock)) {
                        auto &spawnTransform = e.Get<ecs::Transform>(lock);
                        player.Set<ecs::Transform>(lock, spawnTransform);
                    }
                }
            }
        }

        if (scene) {
            for (auto &line : scene->autoExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }

    void GameLogic::ReloadScene(string arg) {
        if (scene) {
            if (arg == "reset") {
                LoadPlayer();
                LoadScene(scene->name);
            } else {
                LoadScene(scene->name);
            }
        }
    }

    void GameLogic::PrintDebug() {
        Logf("Currently loaded scene: %s", scene ? scene->name : "none");
        if (!scene) return;
        auto lock = game->entityManager.tecs.StartTransaction<
            ecs::Read<ecs::Name, ecs::Transform, ecs::HumanController, ecs::LightSensor, ecs::SignalOutput>>();
        if (player && player.Has<ecs::Transform, ecs::HumanController>(lock)) {
            auto &transform = player.Get<ecs::Transform>(lock);
            auto &controller = player.Get<ecs::HumanController>(lock);
            auto position = transform.GetPosition();
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            auto pxFeet = controller.pxController->getFootPosition();
            Logf("Player position: [%f, %f, %f], feet: %f", position.x, position.y, position.z, pxFeet.y);
#else
            Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
#endif
            Logf("Player velocity: [%f, %f, %f]", controller.velocity.x, controller.velocity.y, controller.velocity.z);
            Logf("Player on ground: %s", controller.onGround ? "true" : "false");
        } else {
            Logf("Scene has no valid player");
        }

        for (auto ent : lock.EntitiesWith<ecs::LightSensor>()) {
            auto &sensor = ent.Get<ecs::LightSensor>(lock);
            auto &i = sensor.illuminance;

            Logf("Light sensor %s: %f %f %f", ecs::ToString(lock, ent), i.r, i.g, i.b);
        }

        for (auto ent : lock.EntitiesWith<ecs::SignalOutput>()) {
            auto &output = ent.Get<ecs::SignalOutput>(lock);

            Logf("Signal output %s:", ecs::ToString(lock, ent));
            for (auto &[signalName, value] : output.GetSignals()) {
                Logf("  %s: %.2f", signalName, value);
            }
        }
    }

    void GameLogic::SetSignal(string args) {
        std::stringstream stream(args);
        std::string signalStr;
        double value;
        stream >> signalStr;
        stream >> value;

        auto [entityName, signalName] = ecs::ParseSignal(signalStr);

        auto lock = game->entityManager.tecs.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::SignalOutput>>();
        auto entity = ecs::EntityWith<ecs::Name>(lock, entityName);
        if (!entity) {
            Logf("Signal entity %s not found", entityName);
            return;
        }
        if (!entity.Has<ecs::SignalOutput>(lock)) {
            Logf("%s is not a signal output", entityName);
            return;
        }

        auto &signalComp = entity.Get<ecs::SignalOutput>(lock);
        signalComp.SetSignal(signalName, value);
    }

    void GameLogic::ClearSignal(string args) {
        std::stringstream stream(args);
        std::string signalStr;
        double value;
        stream >> signalStr;
        stream >> value;

        auto [entName, signalName] = ecs::ParseSignal(signalStr);

        auto lock = game->entityManager.tecs.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::SignalOutput>>();
        auto entity = ecs::EntityWith<ecs::Name>(lock, entName);
        if (entity && entity.Has<ecs::SignalOutput>(lock)) {
            auto &signalComp = entity.Get<ecs::SignalOutput>(lock);
            signalComp.ClearSignal(signalName);
        }
    }
} // namespace sp
