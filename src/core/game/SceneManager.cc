#include "SceneManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "console/Console.hh"
#include "console/ConsoleBindingManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <algorithm>
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
        : RegisteredThread("SceneManager", std::chrono::milliseconds(1000)), liveWorld(liveWorld),
          stagingWorld(stagingWorld) {
        funcs.Register(this, "loadscene", "Load a scene and replace current scenes", &SceneManager::LoadScene);
        funcs.Register<std::string>("addscene", "Load a scene", [this](std::string sceneName) {
            AddScene(sceneName, SceneType::User);
        });
        funcs.Register(this, "removescene", "Remove a scene", &SceneManager::RemoveScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &SceneManager::ReloadScene);
        funcs.Register(this, "respawn", "Respawn the player", &SceneManager::RespawnPlayer);
        funcs.Register(this, "printscene", "Print info about currently loaded scenes", &SceneManager::PrintScene);

        StartThread();
    }

    SceneManager::~SceneManager() {
        StopThread();

        for (auto &list : scenes) {
            list.clear();
        }
        playerScene.reset();
        bindingsScene.reset();
        stagedScenes.DropAll();
    }

    void SceneManager::Frame() {
        std::vector<std::string> requiredList;

        {
            auto lock = liveWorld.StartTransaction<ecs::Read<ecs::Name,
                ecs::SceneConnection,
                ecs::SignalOutput,
                ecs::SignalBindings,
                ecs::FocusLayer,
                ecs::FocusLock>>();

            for (auto &ent : lock.EntitiesWith<ecs::SceneConnection>()) {
                auto loadSignal = ecs::SignalBindings::GetSignal(lock, ent, "load_scene_connection");
                if (loadSignal >= 0.5) {
                    auto &connection = ent.Get<ecs::SceneConnection>(lock);
                    for (auto &sceneName : connection.scenes) {
                        if (std::find(requiredList.begin(), requiredList.end(), sceneName) == requiredList.end()) {
                            requiredList.emplace_back(sceneName);
                        }
                    }
                }
            }
        }

        {
            std::shared_lock lock(liveMutex);

            scenes[SceneType::Async].clear();
            for (auto &sceneName : requiredList) {
                auto loadedScene = stagedScenes.Load(sceneName);
                if (loadedScene) {
                    if (loadedScene->type == SceneType::Async) scenes[SceneType::Async].push_back(loadedScene);
                } else {
                    AddScene(sceneName, SceneType::Async);
                }
            }
        }

        stagedScenes.Tick(this->interval);
    }

    void SceneManager::LoadSceneJson(const std::string &sceneName,
        SceneType sceneType,
        ecs::SceneInfo::Priority priority,
        std::function<void(std::shared_ptr<Scene>)> callback) {
        Logf("Loading scene: %s", sceneName);

        auto asset = GAssets.Load("scenes/" + sceneName + ".json", true);
        if (!asset) {
            Logf("Scene not found");
            return;
        }

        std::thread([this, asset, sceneName, sceneType, priority, callback]() {
            asset->WaitUntilValid();
            picojson::value root;
            string err = picojson::parse(root, asset->String());
            if (!err.empty()) {
                Errorf("Failed to parse scene (%s): %s", sceneName, err);
                return;
            }

            auto scene = make_shared<Scene>(sceneName, sceneType, asset);

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

            auto scene = make_shared<Scene>("bindings", SceneType::System);

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

    void SceneManager::TranslateSceneByConnection(const std::shared_ptr<Scene> &scene) {
        auto stagingLock =
            stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo, ecs::Transform, ecs::Animation>,
                ecs::Write<ecs::Transform, ecs::Animation>>();
        auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::Transform>>();

        Tecs::Entity liveConnection, stagingConnection;
        for (auto &e : stagingLock.EntitiesWith<ecs::SceneConnection>()) {
            if (!e.Has<ecs::SceneConnection, ecs::SceneInfo, ecs::Name>(stagingLock)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
            if (sceneInfo.scene.lock() != scene) continue;

            auto &name = e.Get<const ecs::Name>(stagingLock);
            liveConnection = ecs::EntityWith<ecs::Name>(liveLock, name);
            if (liveConnection.Has<ecs::SceneConnection, ecs::Transform>(liveLock)) {
                stagingConnection = e;
                break;
            }
        }
        if (stagingConnection.Has<ecs::Transform>(stagingLock) && liveConnection.Has<ecs::Transform>(liveLock)) {
            auto &liveTransform = liveConnection.Get<const ecs::Transform>(liveLock);
            auto &stagingTransform = stagingConnection.Get<const ecs::Transform>(stagingLock);
            glm::quat deltaRotation = liveTransform.GetGlobalRotation(liveLock) *
                                      glm::inverse(stagingTransform.GetGlobalRotation(stagingLock));
            glm::vec3 deltaPos = liveTransform.GetGlobalPosition(liveLock) -
                                 deltaRotation * stagingTransform.GetGlobalPosition(stagingLock);

            for (auto &e : stagingLock.EntitiesWith<ecs::Transform>()) {
                if (!e.Has<ecs::Transform, ecs::SceneInfo>(stagingLock)) continue;
                auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
                if (sceneInfo.scene.lock() != scene) continue;

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
    }

    void SceneManager::AddSystemScene(std::string sceneName,
        std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)> callback) {
        std::shared_lock lock(liveMutex);

        Assertf(stagedScenes.Load(sceneName) == nullptr, "System scene added with duplicate name: %s", sceneName);
        auto scene = scenes[SceneType::System].emplace_back(std::make_shared<Scene>(sceneName, SceneType::System));
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            callback(stagingLock, scene);

            scene->namedEntities.clear();
            for (auto &e : stagingLock.EntitiesWith<ecs::SceneInfo>()) {
                auto &sceneInfo = e.Get<const ecs::SceneInfo>(stagingLock);
                if (sceneInfo.scene.lock() != scene) continue;

                if (e.Has<ecs::Name>(stagingLock)) {
                    auto &name = e.Get<const ecs::Name>(stagingLock);
                    scene->namedEntities.emplace(name, e);
                }
            }
        }
        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
            auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();

            scene->ApplyScene(stagingLock, liveLock);
        }
    }

    void SceneManager::LoadScene(std::string sceneName) {
        std::shared_lock lock(liveMutex);

        // Unload all current scenes
        scenes[SceneType::Async].clear();
        scenes[SceneType::User].clear();
        stagedScenes.DropAll();

        std::atomic_flag loaded;
        AddScene(sceneName, SceneType::User, [this, &loaded](std::shared_ptr<Scene> scene) {
            RespawnPlayer();

            loaded.test_and_set();
            loaded.notify_all();
        });
        while (!loaded.test()) {
            loaded.wait(false);
        }
    }

    void SceneManager::ReloadScene(std::string sceneName) {
        std::shared_lock lock(liveMutex);

        if (sceneName.empty()) {
            // Reload all async and user scenes
            std::vector<std::pair<std::string, SceneType>> reloadScenes;
            reloadScenes.reserve(scenes[SceneType::Async].size() + scenes[SceneType::User].size());

            for (auto &scene : scenes[SceneType::Async]) {
                reloadScenes.emplace_back(scene->name, SceneType::Async);
            }
            scenes[SceneType::Async].clear();

            for (auto &scene : scenes[SceneType::User]) {
                reloadScenes.emplace_back(scene->name, SceneType::User);
            }
            scenes[SceneType::User].clear();

            stagedScenes.DropAll();
            for (auto &[name, type] : reloadScenes) {
                AddScene(name, type);
            }
        } else {
            auto loadedScene = stagedScenes.Load(sceneName);
            if (loadedScene != nullptr) {
                auto sceneType = loadedScene->type;
                auto &sceneList = scenes[sceneType];
                sceneList.erase(std::remove(sceneList.begin(), sceneList.end(), loadedScene));
                loadedScene.reset();
                Assert(stagedScenes.Drop(sceneName), "Staged scene still in use after removal");

                AddScene(sceneName, sceneType);
            } else {
                Errorf("Scene not current loaded: %s", sceneName);
            }
        }
    }

    void SceneManager::AddScene(std::string sceneName,
        SceneType sceneType,
        std::function<void(std::shared_ptr<Scene>)> callback) {
        std::shared_lock lock(liveMutex);

        auto loadedScene = stagedScenes.Load(sceneName);
        if (loadedScene != nullptr) {
            Logf("Scene %s already loaded", sceneName);
            return;
        }

        LoadSceneJson(sceneName,
            sceneType,
            ecs::SceneInfo::Priority::Scene,
            [this, sceneName, sceneType, callback](std::shared_ptr<Scene> scene) {
                if (scene) {
                    TranslateSceneByConnection(scene);
                    {
                        auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                        auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();

                        scene->ApplyScene(stagingLock, liveLock);
                    }

                    stagedScenes.Register(sceneName, scene);
                    scenes[sceneType].emplace_back(scene);

                    if (callback) callback(scene);
                } else {
                    Errorf("Failed to load scene: %s", sceneName);
                }
            });
    }

    void SceneManager::RemoveScene(std::string sceneName) {
        std::shared_lock lock(liveMutex);

        auto loadedScene = stagedScenes.Load(sceneName);
        if (loadedScene != nullptr) {
            auto &sceneList = scenes[loadedScene->type];
            sceneList.erase(std::remove(sceneList.begin(), sceneList.end(), loadedScene));
            loadedScene.reset();
            Assert(stagedScenes.Drop(sceneName), "Staged scene still in use after removal");
        }
    }

    Tecs::Entity SceneManager::LoadPlayer() {
        std::shared_lock lock(liveMutex);

        playerScene.reset();
        player = Tecs::Entity();

        std::atomic_flag loaded;
        LoadSceneJson("player",
            SceneType::System,
            ecs::SceneInfo::Priority::Player,
            [this, &loaded](std::shared_ptr<Scene> scene) {
                playerScene = scene;
                if (playerScene) {
                    {
                        auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                        auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();

                        playerScene->ApplyScene(stagingLock, liveLock);

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
        return player;
    }

    void SceneManager::RespawnPlayer() {
        std::shared_lock lock(liveMutex);

        auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();

        auto spawn = ecs::EntityWith<ecs::Name>(liveLock, "global.spawn");
        if (spawn.Has<ecs::Transform>(liveLock)) {
            auto &spawnTransform = spawn.Get<ecs::Transform>(liveLock);
            auto spawnPosition = spawnTransform.GetGlobalPosition(liveLock);
            auto spawnRotation = spawnTransform.GetGlobalRotation(liveLock);
            if (player.Has<ecs::Transform>(liveLock)) {
                auto &playerTransform = player.Get<ecs::Transform>(liveLock);
                playerTransform.SetPosition(spawnPosition);
                playerTransform.SetRotation(spawnRotation);
                playerTransform.UpdateCachedTransform(liveLock);
            }
            auto vrOrigin = ecs::EntityWith<ecs::Name>(liveLock, "player.vr-origin");
            if (vrOrigin.Has<ecs::Transform>(liveLock)) {
                auto &vrTransform = vrOrigin.Get<ecs::Transform>(liveLock);
                vrTransform.SetPosition(spawnPosition);
                vrTransform.SetRotation(spawnRotation);
                vrTransform.UpdateCachedTransform(liveLock);
            }
        }
    }

    void SceneManager::LoadBindings() {
        std::shared_lock lock(liveMutex);

        // TODO: Remove console key bindings
        bindingsScene.reset();

        std::atomic_flag loaded;
        LoadBindingsJson([this, &loaded](std::shared_ptr<Scene> scene) {
            bindingsScene = scene;
            if (bindingsScene) {
                auto stagingLock = stagingWorld.StartTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();

                bindingsScene->ApplyScene(stagingLock, liveLock);
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

    void SceneManager::PrintScene(std::string filterName) {
        std::shared_lock lock(liveMutex);
        to_lower(filterName);

        {
            auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
            auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();

            if (filterName.empty() || filterName == "player") {
                Logf("Player scene entities:");
                for (auto &e : liveLock.EntitiesWith<ecs::Name>()) {
                    if (e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) {
                        auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                        auto scene = sceneInfo.scene.lock();
                        if (scene && scene == playerScene) {
                            Logf("  %s", ecs::ToString(liveLock, e));
                            auto stagingId = sceneInfo.nextStagingId;
                            while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                auto stagingScene = stagingInfo.scene.lock();
                                Assert(stagingScene != nullptr, "Missing SceneInfo scene on player entity");
                                Logf("  -> %s scene", stagingScene->name);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }

            if (filterName.empty() || filterName == "bindings") {
                Logf("Binding scene entities:");
                for (auto &e : liveLock.EntitiesWith<ecs::Name>()) {
                    if (e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) {
                        auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                        auto scene = sceneInfo.scene.lock();
                        if (scene && scene == bindingsScene) {
                            Logf("  %s", ecs::ToString(liveLock, e));
                            auto stagingId = sceneInfo.nextStagingId;
                            while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                auto stagingScene = stagingInfo.scene.lock();
                                Assert(stagingScene != nullptr, "Missing SceneInfo scene on binding entity");
                                Logf("  -> %s scene", stagingScene->name);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }

            for (size_t sceneTypeI = 0; sceneTypeI < scenes.size(); sceneTypeI++) {
                auto sceneType = static_cast<SceneType>(sceneTypeI);
                std::string typeName(logging::stringify<SceneType>::to_string(sceneType));
                to_lower(typeName);

                if (filterName.empty() || filterName == typeName) {
                    for (auto scene : scenes[sceneType]) {
                        Logf("Entities from %s scene: %s", typeName, scene->name);
                        for (auto &e : liveLock.EntitiesWith<ecs::Name>()) {
                            if (e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) {
                                auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                                auto liveScene = sceneInfo.scene.lock();
                                if (liveScene && liveScene == scene) {
                                    Logf("  %s", ecs::ToString(liveLock, e));
                                    auto stagingId = sceneInfo.nextStagingId;
                                    while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                        auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                        auto stagingScene = stagingInfo.scene.lock();
                                        Assert(stagingScene != nullptr, "Missing SceneInfo scene on entity");
                                        Logf("  -> %s scene (%s type)", stagingScene->name, stagingScene->type);
                                        stagingId = stagingInfo.nextStagingId;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace sp
