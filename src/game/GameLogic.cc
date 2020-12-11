#define _USE_MATH_DEFINES
#include "game/GameLogic.hh"

#include "assets/AssetManager.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "physx/PhysxUtils.hh"
#include "xr/XrSystemFactory.hh"

#include <cmath>
#include <cxxopts.hpp>
#include <ecs/EcsImpl.hh>
#include <ecs/Signals.hh>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace sp {
    GameLogic::GameLogic(Game *game)
        : game(game), input(&game->input), humanControlSystem(&game->entityManager, &game->input, &game->physics) {
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
        if (game->options.count("map")) { LoadScene(game->options["map"].as<string>()); }

        if (startupScript != nullptr) {
            startupScript->Exec();
        } else if (!game->options.count("map")) {
            LoadScene("menu");
        }

        if (input != nullptr) {
            input->BindCommand(INPUT_ACTION_SET_VR_ORIGIN, "setvrorigin");
            input->BindCommand(INPUT_ACTION_RELOAD_SCENE, "reloadscene");
            input->BindCommand(INPUT_ACTION_RESET_SCENE, "reloadscene reset");
            input->BindCommand(INPUT_ACTION_RELOAD_SHADERS, "reloadshaders");
            input->BindCommand(INPUT_ACTION_TOGGLE_FLASHLIGH, "toggle r.FlashlightOn");
        }
    }

    GameLogic::~GameLogic() {}

    void GameLogic::HandleInput() {
        if (input->FocusLocked()) return;

        if (game->menuGui && input->IsPressed(INPUT_ACTION_OPEN_MENU)) {
            game->menuGui->OpenPauseMenu();
        } else if (input->IsPressed(INPUT_ACTION_SPAWN_DEBUG)) {
            // Spawn dodecahedron
            auto entity = CreateGameLogicEntity();
            auto model = GAssets.LoadModel("dodecahedron");
            entity.Assign<ecs::Renderable>(model);
            entity.Assign<ecs::Transform>(glm::vec3(0, 5, 0));

            PhysxActorDesc desc;
            desc.transform = physx::PxTransform(physx::PxVec3(0, 5, 0));
            auto actor = game->physics.CreateActor(model, desc, entity.GetId());

            if (actor) { entity.Assign<ecs::Physics>(actor, model, desc); }
        } else if (input->IsPressed(INPUT_ACTION_DROP_FLASHLIGH)) {
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

    bool GameLogic::Frame(double dtSinceLastFrame) {
        if (input != nullptr) { HandleInput(); }

        if (!scene) return true;
        ecs::Entity player = GetPlayer();
        if (!player.Valid()) return true;

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
            auto light = ecs::Entity(&game->entityManager, flashlight).Get<ecs::Light>();
            light->intensity = CVarFlashlight.Get(true);
        }
        if (CVarFlashlightOn.Changed()) {
            auto light = ecs::Entity(&game->entityManager, flashlight).Get<ecs::Light>();
            light->on = CVarFlashlightOn.Get(true);
        }
        if (CVarFlashlightAngle.Changed()) {
            auto light = ecs::Entity(&game->entityManager, flashlight).Get<ecs::Light>();
            light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
        }
        if (CVarFlashlightResolution.Changed()) {
            auto view = ecs::Entity(&game->entityManager, flashlight).Get<ecs::View>();
            view->SetProjMat(view->GetFov(), view->GetClip(), glm::ivec2(CVarFlashlightResolution.Get(true)));
        }

        if (!humanControlSystem.Frame(dtSinceLastFrame)) return false;

        return true;
    }

    void GameLogic::LoadScene(string name) {
        game->graphics.RenderLoading();
        game->physics.StopSimulation();
        game->entityManager.DestroyAllWith<ecs::Owner>(ecs::OwnerType::GAME_LOGIC);

        if (scene != nullptr) {
            for (auto &line : scene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        scene.reset();
        {
            auto lock = game->entityManager.tecs.StartTransaction<ecs::AddRemove>();
            scene = GAssets.LoadScene(name, lock, game->physics, ecs::OwnerType::GAME_LOGIC);
        }
        if (!scene) {
            game->physics.StartSimulation();
            return;
        }

        ecs::Entity player = GetPlayer();
        humanControlSystem.AssignController(player, game->physics);
        player.Assign<ecs::VoxelInfo>();

        // Mark the player as being able to activate trigger areas
        player.Assign<ecs::Triggerable>();

        game->graphics.AddPlayerView(player);

        for (auto &line : scene->autoExecList) {
            GetConsoleManager().ParseAndExecute(line);
        }

        // Create flashlight entity
        auto flashlight = CreateGameLogicEntity();
        auto transform = flashlight.Assign<ecs::Transform>(glm::vec3(0, -0.3, 0));
        transform->SetParent(player.GetId());
        auto light = flashlight.Assign<ecs::Light>();
        light->tint = glm::vec3(1.0);
        light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
        light->intensity = CVarFlashlight.Get(true);
        light->on = CVarFlashlightOn.Get(true);
        auto view = flashlight.Assign<ecs::View>();
        view->fov = light->spotAngle * 2.0f;
        view->extents = glm::vec2(CVarFlashlightResolution.Get(true));
        view->clip = glm::vec2(0.1, 64);

        this->flashlight = flashlight.GetEntity();

        // Make sure all objects are in the correct physx state before restarting simulation
        game->physics.LogicFrame();
        game->physics.StartSimulation();
    }

    void GameLogic::ReloadScene(string arg) {
        if (scene) {
            auto player = GetPlayer();
            if (arg == "reset") {
                LoadScene(scene->name);
            } else if (player && player.Has<ecs::Transform>()) {
                // Store the player position and set it back on the new player entity
                auto transform = player.Get<ecs::Transform>();
                auto position = transform->GetPosition();
                auto rotation = transform->GetRotate();

                LoadScene(scene->name);

                if (scene) {
                    player = GetPlayer();
                    if (player && player.Has<ecs::HumanController>()) {
                        humanControlSystem.Teleport(player, position, rotation);
                    }
                }
            }
        }
    }

    string entityName(ecs::Entity ent) {
        string name = ent.ToString();

        if (ent.Has<ecs::Name>()) { name += " (" + *ent.Get<ecs::Name>() + ")"; }
        return name;
    }

    void GameLogic::PrintDebug() {
        Logf("Currently loaded scene: %s", scene ? scene->name : "none");
        if (!scene) return;
        auto player = GetPlayer();
        if (player && player.Has<ecs::Transform>() && player.Has<ecs::HumanController>()) {
            auto transform = player.Get<ecs::Transform>();
            auto controller = player.Get<ecs::HumanController>();
            auto position = transform->GetPosition();
            auto pxFeet = controller->pxController->getFootPosition();
            Logf("Player position: [%f, %f, %f], feet: %f", position.x, position.y, position.z, pxFeet.y);
            Logf("Player velocity: [%f, %f, %f]",
                 controller->velocity.x,
                 controller->velocity.y,
                 controller->velocity.z);
            Logf("Player on ground: %s", controller->onGround ? "true" : "false");
        } else {
            Logf("Scene has no valid player");
        }

        for (auto ent : game->entityManager.EntitiesWith<ecs::LightSensor>()) {
            auto sensor = ent.Get<ecs::LightSensor>();
            auto i = sensor->illuminance;
            string name = entityName(ent);

            Logf("Light sensor %s: %f %f %f", name, i.r, i.g, i.b);
        }

        for (auto ent : game->entityManager.EntitiesWith<ecs::SignalOutput>()) {
            auto output = ent.Get<ecs::SignalOutput>();
            string name = entityName(ent);

            Logf("Signal output %s:", name);
            for (auto &[signalName, value] : output->GetSignals()) {
                Logf("  %s: %.2f", signalName, value);
            }
        }
    }

    ecs::Entity GameLogic::GetPlayer() {
        return game->entityManager.EntityWith<ecs::Name>("player");
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

    /**
     * @brief Helper function used when creating new entities that belong to
     * 		  GameLogic. Using this function ensures that the correct ecs::Creator
     * 		  attribute is added to each entity that is owned by GameLogic, and
     *		  therefore it gets destroyed on scene unload.
     *
     * @return ecs::Entity A new Entity with the ecs::Creator::GAME_LOGIC Key added.
     */
    inline ecs::Entity GameLogic::CreateGameLogicEntity() {
        auto newEntity = game->entityManager.NewEntity();
        newEntity.Assign<ecs::Owner>(ecs::OwnerType::GAME_LOGIC);
        return newEntity;
    }
} // namespace sp
