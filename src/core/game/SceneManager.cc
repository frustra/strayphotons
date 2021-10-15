#include "SceneManager.hh"

#include "assets/AssetManager.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace sp {
    SceneManager::SceneManager() {
        funcs.Register(this, "loadscene", "Load a scene", &SceneManager::LoadScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &SceneManager::ReloadScene);
        funcs.Register(this, "reloadplayer", "Reload player scene", &SceneManager::ReloadPlayer);
        funcs.Register(this, "printscene", "Print info about currently scenes", &SceneManager::PrintScene);
    }

    bool SceneManager::Frame(double dtSinceLastFrame) {
        if (!scene) return true;

        {
            auto lock = ecs::World.StartTransaction<ecs::WriteAll>();
            for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
                auto &script = entity.Get<ecs::Script>(lock);
                script.OnTick(lock, entity, dtSinceLastFrame);
            }

            for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
                auto &area = entity.Get<ecs::TriggerArea>(lock);

                if (!area.triggered) {
                    for (auto triggerableEntity : lock.EntitiesWith<ecs::Triggerable>()) {
                        auto &transform = triggerableEntity.Get<ecs::Transform>(lock);
                        auto entityPos = transform.GetPosition();
                        if (entityPos.x > area.boundsMin.x && entityPos.y > area.boundsMin.y &&
                            entityPos.z > area.boundsMin.z && entityPos.x < area.boundsMax.x &&
                            entityPos.y < area.boundsMax.y && entityPos.z < area.boundsMax.z) {
                            Debugf("Entity at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
                            Logf("Triggering event: %s", area.command);
                            area.triggered = true;
                            GetConsoleManager().QueueParseAndExecute(area.command);
                            break;
                        }
                    }
                }
            }
        }

        if (!scene) return true;

        return true;
    }

    void SceneManager::LoadPlayer() {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::Owner>(lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));
        }

        if (playerScene != nullptr) {
            for (auto &line : playerScene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        playerScene.reset();
        player = Tecs::Entity();

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            playerScene = GAssets.LoadScene("player", lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                if (e.Has<ecs::Owner>(lock)) {
                    auto &name = e.Get<ecs::Name>(lock);
                    auto &owner = e.Get<ecs::Owner>(lock);
                    if (owner == ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0)) {
                        if (name == "player") {
                            player = e;
                            break;
                        }
                    }
                }
            }
            Assert(!!player, "Player scene doesn't contain an entity named player");

            auto spawn = ecs::EntityWith<ecs::Name>(lock, "player-spawn");
            auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "vr-origin");
            if (spawn.Has<ecs::Transform>(lock)) {
                auto &spawnTransform = spawn.Get<ecs::Transform>(lock);
                auto spawnPosition = spawnTransform.GetGlobalPosition(lock);
                auto spawnRotation = spawnTransform.GetGlobalRotation(lock);
                auto &playerTransform = player.Get<ecs::Transform>(lock);
                playerTransform.SetPosition(spawnPosition);
                playerTransform.SetRotation(spawnRotation);
                playerTransform.UpdateCachedTransform(lock);
                if (vrOrigin) {
                    auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                    vrTransform.SetPosition(spawnPosition);
                    vrTransform.SetRotation(spawnRotation);
                    vrTransform.UpdateCachedTransform(lock);
                }
            } else if (!player.Has<ecs::Transform>(lock)) {
                player.Set<ecs::Transform>(lock, glm::vec3(0));
            }

            // Mark the player as being able to activate trigger areas
            player.Set<ecs::Triggerable>(lock);
            // Allow the player to be a parent for constraints
            player.Set<ecs::InteractController>(lock);
        }

        if (playerScene) {
            for (auto &line : playerScene->autoExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }

    void SceneManager::LoadScene(std::string sceneName) {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::Owner>(lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));
        }

        if (scene != nullptr) {
            for (auto &line : scene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        scene.reset();
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            scene = GAssets.LoadScene(sceneName, lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                auto &name = e.Get<ecs::Name>(lock);
                if (name == "player" && e != player) {
                    name = "player-spawn";
                    if (e.Has<ecs::Transform>(lock)) {
                        auto &spawnTransform = e.Get<ecs::Transform>(lock);
                        auto spawnPosition = spawnTransform.GetGlobalPosition(lock);
                        auto spawnRotation = spawnTransform.GetGlobalRotation(lock);
                        auto &playerTransform = player.Get<ecs::Transform>(lock);
                        playerTransform.SetPosition(spawnPosition);
                        playerTransform.SetRotation(spawnRotation);
                        playerTransform.UpdateCachedTransform(lock);

                        auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "vr-origin");
                        if (vrOrigin) {
                            auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                            vrTransform.SetPosition(spawnPosition);
                            vrTransform.SetRotation(spawnRotation);
                            vrTransform.UpdateCachedTransform(lock);
                        }
                    }
                    break;
                }
            }
        }

        if (scene) {
            for (auto &line : scene->autoExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }

    void SceneManager::ReloadScene() {
        if (scene) LoadScene(scene->name);
    }

    void SceneManager::ReloadPlayer() {
        LoadPlayer();

        // TODO: Find a way to handle entity overrides / layers in a generic way
#ifdef SP_XR_SUPPORT
        if (game->options["no-vr"].count() == 0) game->xr.LoadXrSystem();
#endif

#ifdef SP_INPUT_SUPPORT
        game->inputBindingLoader.Load(InputBindingConfigPath);
#endif
    }

    void SceneManager::PrintScene() {
        Logf("Currently loaded scene: %s", scene ? scene->name : "none");
    }
} // namespace sp
