#include "SceneManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "console/Console.hh"
#include "console/ConsoleBindingManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <picojson/picojson.h>

namespace sp {
    SceneManager &GetSceneManager() {
        static ecs::ECS stagingWorld;
        static SceneManager GSceneManager(ecs::World, stagingWorld);
        return GSceneManager;
    }

    SceneManager::SceneManager(ecs::ECS &liveWorld, ecs::ECS &stagingWorld)
        : liveWorld(liveWorld), stagingWorld(stagingWorld) {
        funcs.Register(this, "loadscene", "Load a scene and replace current scenes", &SceneManager::LoadScene);
        funcs.Register<std::string>("addscene", "Load a scene", [this](std::string sceneName) {
            AddScene(sceneName);
        });
        funcs.Register(this, "removescene", "Remove a scene", &SceneManager::RemoveScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &SceneManager::ReloadScene);
        funcs.Register(this, "respawn", "Respawn the player", &SceneManager::RespawnPlayer);
        funcs.Register(this, "printscene", "Print info about currently loaded scenes", &SceneManager::PrintScene);
    }

    void SceneManager::LoadSceneJson(const std::string &sceneName,
        ecs::SceneInfo::Priority priority,
        std::function<void(std::shared_ptr<Scene>)> callback) {
        Logf("Loading scene: %s", sceneName);

        auto asset = GAssets.Load("scenes/" + sceneName + ".json", true);
        if (!asset) {
            Logf("Scene not found");
            return;
        }

        std::thread([this, asset, sceneName, priority, callback]() {
            asset->WaitUntilValid();
            picojson::value root;
            string err = picojson::parse(root, asset->String());
            if (!err.empty()) {
                Errorf("Failed to parse scene (%s): %s", sceneName, err);
                return;
            }

            auto scene = make_shared<Scene>(sceneName, asset);

            {
                auto lock = stagingWorld.StartTransaction<ecs::AddRemove>();

                auto entityList = root.get<picojson::object>()["entities"];
                // Find all named entities first so they can be referenced.
                for (auto value : entityList.get<picojson::array>()) {
                    auto ent = value.get<picojson::object>();

                    if (ent.count("_name")) {
                        Tecs::Entity entity = lock.NewEntity();
                        auto name = ent["_name"].get<string>();
                        if (starts_with(name, "global.")) {
                            entity.Set<ecs::Name>(lock, name);
                        } else {
                            entity.Set<ecs::Name>(lock, sceneName + "." + name);
                        }
                        Assert(scene->namedEntities.count(name) == 0, "Duplicate entity name: " + name);
                        scene->namedEntities.emplace(name, entity);
                    }
                }

                for (auto value : entityList.get<picojson::array>()) {
                    auto ent = value.get<picojson::object>();

                    Tecs::Entity entity;
                    if (ent.count("_name")) {
                        entity = scene->namedEntities[ent["_name"].get<string>()];
                    } else {
                        entity = lock.NewEntity();
                    }

                    entity.Set<ecs::SceneInfo>(lock, entity, priority, scene);
                    for (auto comp : ent) {
                        if (comp.first[0] == '_') continue;

                        auto componentType = ecs::LookupComponent(comp.first);
                        if (componentType != nullptr) {
                            bool result = componentType->LoadEntity(lock, entity, comp.second);
                            Assert(result, "Failed to load component type: " + comp.first);
                        } else {
                            Errorf("Unknown component, ignoring: %s", comp.first);
                        }
                    }
                }
            }
            callback(scene);
        }).detach();
    }

    void SceneManager::LoadBindingsJson(std::function<void(std::shared_ptr<Scene>)> callback) {
        Logf("Loading bindings json: %s", InputBindingConfigPath);

        std::thread([this, callback]() {
            std::string bindingConfig;

            if (!std::filesystem::exists(InputBindingConfigPath)) {
                auto defaultConfigAsset = GAssets.Load("default_input_bindings.json");
                Assert(defaultConfigAsset, "Default input binding config missing");
                defaultConfigAsset->WaitUntilValid();

                bindingConfig = defaultConfigAsset->String();

                std::ofstream file(InputBindingConfigPath, std::ios::binary);
                Assertf(file.is_open(), "Failed to create binding config file: %s", InputBindingConfigPath);
                file << bindingConfig;
                file.close();
            } else {
                std::ifstream file(InputBindingConfigPath);
                Assertf(file.is_open(), "Failed to open binding config file: %s", InputBindingConfigPath);

                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);

                bindingConfig.resize(size);
                file.read(bindingConfig.data(), size);
                file.close();
            }

            auto scene = make_shared<Scene>("bindings");

            picojson::value root;
            string err = picojson::parse(root, bindingConfig);
            if (!err.empty()) Abort("Failed to parse user input_binding.json file: " + err);

            {
                auto lock = stagingWorld.StartTransaction<ecs::AddRemove>();

                for (auto param : root.get<picojson::object>()) {
                    Debugf("Loading input for: %s", param.first);
                    auto entity = lock.NewEntity();
                    entity.Set<ecs::Name>(lock, param.first);
                    entity.Set<ecs::SceneInfo>(lock, entity, ecs::SceneInfo::Priority::Bindings, scene);
                    for (auto comp : param.second.get<picojson::object>()) {
                        if (comp.first[0] == '_') continue;

                        auto componentType = ecs::LookupComponent(comp.first);
                        if (componentType != nullptr) {
                            bool result = componentType->LoadEntity(lock, entity, comp.second);
                            Assert(result, "Failed to load component type: " + comp.first);
                        } else {
                            Errorf("Unknown component, ignoring: %s", comp.first);
                        }
                    }
                }
            }
            callback(scene);
        }).detach();
    }

    void SceneManager::AddSystemScene(std::string sceneName,
        std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)> callback) {
        auto scene = systemScenes.emplace(sceneName, std::make_shared<Scene>(sceneName)).first->second;
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            callback(stagingLock, scene);

            scene->namedEntities.clear();
            for (auto &e : stagingLock.EntitiesWith<ecs::SceneInfo>()) {
                auto &sceneInfo = e.Get<const ecs::SceneInfo>(stagingLock);
                if (sceneInfo.scene != scene) continue;

                if (e.Has<ecs::Name>(stagingLock)) {
                    auto &name = e.Get<const ecs::Name>(stagingLock);
                    scene->namedEntities.emplace(name, e);
                }
            }
        }
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
            auto lock = liveWorld.StartTransaction<ecs::AddRemove>();

            scene->ApplyScene(stagingLock, lock);
        }
    }

    void SceneManager::RemoveSystemScene(std::string sceneName) {
        auto it = systemScenes.find(sceneName);
        if (it != systemScenes.end()) {
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            auto lock = liveWorld.StartTransaction<ecs::AddRemove>();

            it->second->RemoveScene(stagingLock, lock);
            systemScenes.erase(it);
        }
    }

    void SceneManager::LoadScene(std::string sceneName) {
        // Unload all current scenes
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            auto lock = liveWorld.StartTransaction<ecs::AddRemove>();

            for (auto &[name, scene] : scenes) {
                Logf("Unloading scene: %s", name);
                scene->RemoveScene(stagingLock, lock);
            }
            scenes.clear();
        }
        std::atomic_flag loaded;
        AddScene(sceneName, [this, &loaded](std::shared_ptr<Scene> scene) {
            RespawnPlayer();

            loaded.test_and_set();
            loaded.notify_all();
        });
        while (!loaded.test()) {
            loaded.wait(false);
        }
    }

    void SceneManager::ReloadScene(std::string sceneName) {
        if (sceneName.empty()) {
            if (scenes.size() == 0) {
                Errorf("No scenes are currently loaded");
            } else if (scenes.size() == 1) {
                sceneName = scenes.begin()->first;
                RemoveScene(sceneName);
                AddScene(sceneName);
            } else {
                std::stringstream ss;
                for (auto &scene : scenes) {
                    ss << " " << scene.first;
                }
                Errorf("Multiple scenes are loaded, pick one of:%s", ss.str());
            }
        } else {
            auto it = scenes.find(sceneName);
            if (it != scenes.end()) {
                RemoveScene(sceneName);
                AddScene(sceneName);
            } else {
                Errorf("Scene not current loaded: %s", sceneName);
            }
        }
    }

    void SceneManager::AddScene(std::string sceneName, std::function<void(std::shared_ptr<Scene>)> callback) {
        auto it = scenes.find(sceneName);
        if (it != scenes.end()) {
            Logf("Scene %s already loaded", sceneName);
            return;
        }

        LoadSceneJson(sceneName,
            ecs::SceneInfo::Priority::Scene,
            [this, sceneName, callback](std::shared_ptr<Scene> scene) {
                if (scene) {
                    {
                        auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll,
                            ecs::Write<ecs::SceneInfo, ecs::Transform, ecs::Animation>>();
                        auto lock = liveWorld.StartTransaction<ecs::AddRemove>();

                        Tecs::Entity liveConnection, stagingConnection;
                        for (auto &e : stagingLock.EntitiesWith<ecs::SceneConnection>()) {
                            if (!e.Has<ecs::SceneConnection, ecs::SceneInfo, ecs::Name>(stagingLock)) continue;
                            auto &sceneInfo = e.Get<const ecs::SceneInfo>(stagingLock);
                            if (sceneInfo.scene != scene) continue;

                            auto &name = e.Get<const ecs::Name>(stagingLock);
                            liveConnection = ecs::EntityWith<ecs::Name>(lock, name);
                            if (liveConnection.Has<ecs::SceneConnection, ecs::Transform>(lock)) {
                                stagingConnection = e;
                                break;
                            }
                        }
                        if (stagingConnection.Has<ecs::Transform>(stagingLock) &&
                            liveConnection.Has<ecs::Transform>(lock)) {
                            auto &liveTransform = liveConnection.Get<const ecs::Transform>(lock);
                            auto &stagingTransform = stagingConnection.Get<const ecs::Transform>(stagingLock);
                            glm::quat deltaRotation = liveTransform.GetGlobalRotation(lock) *
                                                      glm::inverse(stagingTransform.GetGlobalRotation(stagingLock));
                            glm::vec3 deltaPos = liveTransform.GetGlobalPosition(lock) -
                                                 deltaRotation * stagingTransform.GetGlobalPosition(stagingLock);

                            for (auto &e : stagingLock.EntitiesWith<ecs::Transform>()) {
                                if (!e.Has<ecs::Transform, ecs::SceneInfo>(stagingLock)) continue;
                                auto &sceneInfo = e.Get<const ecs::SceneInfo>(stagingLock);
                                if (sceneInfo.scene != scene) continue;

                                auto &transform = e.Get<ecs::Transform>(stagingLock);
                                if (!transform.HasParent(stagingLock)) {
                                    transform.SetPosition(deltaRotation * transform.GetPosition() + deltaPos);
                                    transform.SetRotation(deltaRotation * transform.GetRotation());

                                    if (e.Has<ecs::Animation>(stagingLock)) {
                                        auto &animation = e.Get<ecs::Animation>(stagingLock);
                                        for (auto &state : animation.states) {
                                            state.pos += deltaPos;
                                        }
                                    }
                                }
                            }
                        }

                        scene->ApplyScene(stagingLock, lock);
                        scenes.emplace(sceneName, scene);
                    }

                    if (callback) callback(scene);
                } else {
                    Errorf("Failed to load scene: %s", sceneName);
                }
            });
    }

    void SceneManager::RemoveScene(std::string sceneName) {
        auto it = scenes.find(sceneName);
        if (it != scenes.end()) {
            Logf("Unloading scene: %s", it->first);
            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
                auto lock = liveWorld.StartTransaction<ecs::AddRemove>();
                it->second->RemoveScene(stagingLock, lock);
            }

            scenes.erase(it);
        }
    }

    void SceneManager::LoadPlayer() {
        if (playerScene) {
            Logf("Unloading player");
            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
                auto lock = liveWorld.StartTransaction<ecs::AddRemove>();
                playerScene->RemoveScene(stagingLock, lock);
            }
            playerScene.reset();
        }

        player = Tecs::Entity();

        std::atomic_flag loaded;
        LoadSceneJson("player", ecs::SceneInfo::Priority::Player, [this, &loaded](std::shared_ptr<Scene> scene) {
            playerScene = scene;
            if (playerScene) {
                {
                    auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                    auto lock = liveWorld.StartTransaction<ecs::AddRemove>();

                    playerScene->ApplyScene(stagingLock, lock);

                    auto it = playerScene->namedEntities.find("player");
                    if (it != playerScene->namedEntities.end()) {
                        auto stagingPlayer = it->second;
                        if (stagingPlayer && stagingPlayer.Has<ecs::SceneInfo>(stagingLock)) {
                            auto &sceneInfo = stagingPlayer.Get<ecs::SceneInfo>(stagingLock);
                            player = sceneInfo.liveId;
                        }
                    }
                    Assert(!!player, "Player scene doesn't contain an entity named player");
                }
            } else {
                Errorf("Failed to load player scene!");
            }

            loaded.test_and_set();
            loaded.notify_all();
        });
        while (!loaded.test()) {
            loaded.wait(false);
        }
        Assert(!!player, "Player scene doesn't contain an entity named player");
    }

    void SceneManager::RespawnPlayer() {
        auto lock = liveWorld.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();

        auto spawn = ecs::EntityWith<ecs::Name>(lock, "global.spawn");
        if (spawn.Has<ecs::Transform>(lock)) {
            auto &spawnTransform = spawn.Get<ecs::Transform>(lock);
            auto spawnPosition = spawnTransform.GetGlobalPosition(lock);
            auto spawnRotation = spawnTransform.GetGlobalRotation(lock);
            if (player.Has<ecs::Transform>(lock)) {
                auto &playerTransform = player.Get<ecs::Transform>(lock);
                playerTransform.SetPosition(spawnPosition);
                playerTransform.SetRotation(spawnRotation);
                playerTransform.UpdateCachedTransform(lock);
            }
            auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "player.vr-origin");
            if (vrOrigin.Has<ecs::Transform>(lock)) {
                auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                vrTransform.SetPosition(spawnPosition);
                vrTransform.SetRotation(spawnRotation);
                vrTransform.UpdateCachedTransform(lock);
            }
        }
    }

    void SceneManager::LoadBindings() {
        if (bindingsScene) {
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            auto lock = liveWorld.StartTransaction<ecs::AddRemove>();
            bindingsScene->RemoveScene(stagingLock, lock);
            // TODO: Remove console key bindings
            bindingsScene.reset();
        }

        std::atomic_flag loaded;
        LoadBindingsJson([this, &loaded](std::shared_ptr<Scene> scene) {
            bindingsScene = scene;
            if (bindingsScene) {
                auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                auto lock = liveWorld.StartTransaction<ecs::AddRemove>();

                bindingsScene->ApplyScene(stagingLock, lock);
            } else {
                Errorf("Failed to load bindings scene!");
            }

            loaded.test_and_set();
            loaded.notify_all();
        });
        while (!loaded.test()) {
            loaded.wait(false);
        }
    }

    void SceneManager::PrintScene(std::string sceneName) {
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
            auto lock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();

            for (auto &[systemName, systemScene] : systemScenes) {
                if (sceneName.empty() || sceneName == systemName) {
                    Logf("System scene (%s) entities:", systemName);
                    for (auto &e : lock.EntitiesWith<ecs::Name>()) {
                        if (e.Has<ecs::Name, ecs::SceneInfo>(lock)) {
                            auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                            if (sceneInfo.scene && sceneInfo.scene == systemScene) {
                                Logf("  %s", ecs::ToString(lock, e));
                                auto stagingId = sceneInfo.nextStagingId;
                                while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                    auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                    Assert(stagingInfo.scene != nullptr, "Missing SceneInfo scene on system entity");
                                    Logf("  -> %s scene", stagingInfo.scene->sceneName);
                                    stagingId = stagingInfo.nextStagingId;
                                }
                            }
                        }
                    }
                }
            }

            if (sceneName.empty() || sceneName == "player") {
                Logf("Player scene entities:");
                for (auto &e : lock.EntitiesWith<ecs::Name>()) {
                    if (e.Has<ecs::Name, ecs::SceneInfo>(lock)) {
                        auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                        if (sceneInfo.scene && sceneInfo.scene == playerScene) {
                            Logf("  %s", ecs::ToString(lock, e));
                            auto stagingId = sceneInfo.nextStagingId;
                            while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                Assert(stagingInfo.scene != nullptr, "Missing SceneInfo scene on player entity");
                                Logf("  -> %s scene", stagingInfo.scene->sceneName);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }

            if (sceneName.empty() || sceneName == "bindings") {
                Logf("Binding scene entities:");
                for (auto &e : lock.EntitiesWith<ecs::Name>()) {
                    if (e.Has<ecs::Name, ecs::SceneInfo>(lock)) {
                        auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                        if (sceneInfo.scene && sceneInfo.scene == bindingsScene) {
                            Logf("  %s", ecs::ToString(lock, e));
                            auto stagingId = sceneInfo.nextStagingId;
                            while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                Assert(stagingInfo.scene != nullptr, "Missing SceneInfo scene on binding entity");
                                Logf("  -> %s scene", stagingInfo.scene->sceneName);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }

            for (auto &scene : scenes) {
                if (sceneName.empty() || sceneName == scene.first) {
                    Logf("Scene entities (%s):", scene.first);
                    for (auto &e : lock.EntitiesWith<ecs::Name>()) {
                        if (e.Has<ecs::Name, ecs::SceneInfo>(lock)) {
                            auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                            if (sceneInfo.scene && sceneInfo.scene == scene.second) {
                                Logf("  %s", ecs::ToString(lock, e));
                                auto stagingId = sceneInfo.nextStagingId;
                                while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                    auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                    Assertf(stagingInfo.scene != nullptr,
                                        "Missing SceneInfo scene on %s entity",
                                        scene.first);
                                    Logf("  -> %s scene", stagingInfo.scene->sceneName);
                                    stagingId = stagingInfo.nextStagingId;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace sp
