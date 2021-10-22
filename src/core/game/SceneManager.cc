#include "SceneManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "console/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <filesystem>
#include <fstream>
#include <future>
#include <glm/glm.hpp>
#include <picojson/picojson.h>

namespace sp {
    SceneManager::SceneManager() {
        funcs.Register(this, "loadscene", "Load a scene and replace current scenes", &SceneManager::LoadScene);
        funcs.Register(this, "addscene", "Load a scene", &SceneManager::AddScene);
        funcs.Register(this, "removescene", "Remove a scene", &SceneManager::RemoveScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &SceneManager::ReloadScene);
        funcs.Register(this, "respawn", "Respawn the player", &SceneManager::RespawnPlayer);
        funcs.Register(this, "printscenes", "Print info about currently loaded scenes", &SceneManager::PrintScenes);
    }

    std::future<std::shared_ptr<Scene>> SceneManager::LoadSceneJson(const std::string &sceneName) {
        Logf("Loading scene: %s", sceneName);

        auto asset = GAssets.Load("scenes/" + sceneName + ".json", true);
        if (!asset) {
            Logf("Scene not found");
            return std::future<std::shared_ptr<Scene>>();
        }

        return std::async(std::launch::async, [this, asset, sceneName]() -> std::shared_ptr<Scene> {
            asset->WaitUntilValid();
            picojson::value root;
            string err = picojson::parse(root, asset->String());
            if (!err.empty()) {
                Errorf("%s", err);
                return nullptr;
            }

            auto scene = make_shared<Scene>(sceneName, asset);

            auto autoExecList = root.get<picojson::object>()["autoexec"];
            if (autoExecList.is<picojson::array>()) {
                for (auto value : autoExecList.get<picojson::array>()) {
                    auto line = value.get<string>();
                    scene->autoExecList.push_back(line);
                }
            }

            auto unloadExecList = root.get<picojson::object>()["unloadexec"];
            if (unloadExecList.is<picojson::array>()) {
                for (auto value : unloadExecList.get<picojson::array>()) {
                    auto line = value.get<string>();
                    scene->unloadExecList.push_back(line);
                }
            }

            {
                auto lock = stagingWorld.StartTransaction<ecs::AddRemove>();

                auto entityList = root.get<picojson::object>()["entities"];
                // Find all named entities first so they can be referenced.
                for (auto value : entityList.get<picojson::array>()) {
                    auto ent = value.get<picojson::object>();

                    if (ent.count("_name")) {
                        Tecs::Entity entity = lock.NewEntity();
                        auto name = ent["_name"].get<string>();
                        entity.Set<ecs::Name>(lock, name);
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

                    entity.Set<ecs::SceneInfo>(lock, entity, scene);
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
            return scene;
        });
    }

    std::shared_ptr<Scene> SceneManager::LoadBindingJson(std::string bindingConfigPath) {
        std::string bindingConfig;

        if (!std::filesystem::exists(bindingConfigPath)) {
            auto defaultConfigAsset = GAssets.Load("default_input_bindings.json");
            Assert(defaultConfigAsset, "Default input binding config missing");
            defaultConfigAsset->WaitUntilValid();

            bindingConfig = defaultConfigAsset->String();

            std::ofstream file(bindingConfigPath, std::ios::binary);
            Assert(file.is_open(), "Failed to create binding config file: " + bindingConfigPath);
            file << bindingConfig;
            file.close();
        } else {
            std::ifstream file(bindingConfigPath);
            Assert(file.is_open(), "Failed to open binding config file: " + bindingConfigPath);

            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            bindingConfig.resize(size);
            file.read(bindingConfig.data(), size);
            file.close();
        }

        picojson::value root;
        string err = picojson::parse(root, bindingConfig);
        if (!err.empty()) Abort("Failed to parse user input_binding.json file: " + err);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            for (auto param : root.get<picojson::object>()) {
                if (param.first == "_console") {
                    Debugf("Loading console-input bindings");
                    for (auto eventInput : param.second.get<picojson::object>()) {
                        auto command = eventInput.second.get<std::string>();
                        consoleBinding.SetConsoleInputCommand(lock, eventInput.first, command);
                    }
                } else {
                    auto entity = ecs::EntityWith<ecs::Name>(lock, param.first);
                    if (entity) {
                        Debugf("Loading input for: %s", param.first);
                        for (auto comp : param.second.get<picojson::object>()) {
                            if (comp.first[0] == '_') continue;

                            auto componentType = ecs::LookupComponent(comp.first);
                            if (componentType != nullptr) {
                                bool result = componentType->ReloadEntity(lock, entity, comp.second);
                                Assert(result, "Failed to load component type: " + comp.first);
                            } else {
                                Errorf("Unknown component, ignoring: %s", comp.first);
                            }
                        }
                    }
                }
            }
        }

        // TODO
        return nullptr;
    }

    void SceneManager::LoadScene(std::string sceneName) {
        // Unload all current scenes
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            for (auto &[name, scene] : scenes) {
                Logf("Unloading scene: %s", name);
                ecs::DestroyAllWith<ecs::SceneInfo>(lock, ecs::SceneInfo(scene));
                ecs::DestroyAllWith<ecs::SceneInfo>(stagingLock, ecs::SceneInfo(scene));

                for (auto &line : scene->unloadExecList) {
                    GetConsoleManager().QueueParseAndExecute(line);
                }
            }
            scenes.clear();
        }
        AddScene(sceneName);

        RespawnPlayer();
        LoadBindingJson(InputBindingConfigPath);
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

    void SceneManager::AddScene(std::string sceneName) {
        auto sceneFuture = LoadSceneJson(sceneName);
        if (sceneFuture.valid()) {
            auto scene = sceneFuture.get();

            if (scene) {
                {
                    auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                    auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

                    scene->CopyScene(stagingLock, lock);
                }

                for (auto &line : scene->autoExecList) {
                    GetConsoleManager().ParseAndExecute(line);
                }

                scenes.emplace(sceneName, scene);
            } else {
                Errorf("Failed to load scene: %s", sceneName);
            }
        } else {
            Errorf("Failed to load scene: %s", sceneName);
        }
    }

    void SceneManager::RemoveScene(std::string sceneName) {
        auto it = scenes.find(sceneName);
        if (it != scenes.end()) {
            Logf("Unloading scene: %s", it->first);
            {
                auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
                ecs::DestroyAllWith<ecs::SceneInfo>(lock, ecs::SceneInfo(it->second));
            }
            {
                auto lock = stagingWorld.StartTransaction<ecs::AddRemove>();
                ecs::DestroyAllWith<ecs::SceneInfo>(lock, ecs::SceneInfo(it->second));
            }

            for (auto &line : it->second->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
            scenes.erase(it);
        }
    }

    void SceneManager::LoadPlayer() {
        if (playerScene) {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::SceneInfo>(lock, ecs::SceneInfo(playerScene));
        }
        if (playerScene) {
            auto lock = stagingWorld.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::SceneInfo>(lock, ecs::SceneInfo(playerScene));
        }
        if (playerScene) {
            for (auto &line : playerScene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        playerScene.reset();
        player = Tecs::Entity();

        auto sceneFuture = LoadSceneJson("player");
        if (sceneFuture.valid()) {
            playerScene = sceneFuture.get();

            if (playerScene) {
                {
                    auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                    auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

                    playerScene->CopyScene(stagingLock, lock);

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

                for (auto &line : playerScene->autoExecList) {
                    GetConsoleManager().ParseAndExecute(line);
                }
            } else {
                Errorf("Failed to load player scene!");
            }
        } else {
            Errorf("Failed to load player scene!");
        }
    }

    void SceneManager::RespawnPlayer() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();

        auto spawn = ecs::EntityWith<ecs::Name>(lock, "player-spawn");
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
            auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "vr-origin");
            if (vrOrigin.Has<ecs::Transform>(lock)) {
                auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                vrTransform.SetPosition(spawnPosition);
                vrTransform.SetRotation(spawnRotation);
                vrTransform.UpdateCachedTransform(lock);
            }
        }
    }

    void SceneManager::PrintScenes() {
        std::stringstream ss;
        for (auto &scene : scenes) {
            ss << " " << scene.first;
        }
        if (scenes.empty()) ss << " none";
        Logf("Currently loaded scenes:%s", ss.str());
        Logf("Player loaded: %s", playerScene ? "yes" : "no");
    }
} // namespace sp
