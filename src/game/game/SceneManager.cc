#include "SceneManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "console/Console.hh"
#include "console/ConsoleBindingManager.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <picojson/picojson.h>
#include <shared_mutex>

namespace sp {
    SceneManager &GetSceneManager() {
        // Ensure the ECS and AssetManager are constructed first so they are destructed in the right order.
        ecs::World();
        ecs::StagingWorld();
        sp::Assets();
        static SceneManager sceneManager;
        return sceneManager;
    }

    SceneManager::SceneManager(bool skipPreload) : RegisteredThread("SceneManager", 30.0), skipPreload(skipPreload) {
        funcs.Register<std::string>("loadscene",
            "Load a scene and replace current scenes",
            [this](std::string sceneName) {
                QueueActionAndBlock(SceneAction::LoadScene, sceneName);
            });
        funcs.Register<std::string>("addscene", "Load a scene", [this](std::string sceneName) {
            QueueActionAndBlock(SceneAction::AddScene, sceneName);
        });
        funcs.Register<std::string>("removescene", "Remove a scene", [this](std::string sceneName) {
            QueueActionAndBlock(SceneAction::RemoveScene, sceneName);
        });
        funcs.Register<std::string>("reloadscene", "Reload current scene", [this](std::string sceneName) {
            QueueActionAndBlock(SceneAction::ReloadScene, sceneName);
        });
        funcs.Register("reloadplayer", "Reload player scene", [this]() {
            QueueActionAndBlock(SceneAction::ReloadPlayer);
        });
        funcs.Register("reloadbindings", "Reload input bindings", [this]() {
            QueueActionAndBlock(SceneAction::ReloadBindings);
        });
        funcs.Register("respawn", "Respawn the player", [this]() {
            auto liveLock =
                ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>>();
            RespawnPlayer(liveLock, player);
        });
        funcs.Register(this, "printscene", "Print info about currently loaded scenes", &SceneManager::PrintScene);

        StartThread();
    }

    SceneManager::~SceneManager() {
        {
            // Make sure we don't deadlock on shutdown due to waiting on a preload.
            std::lock_guard lock(preloadMutex);
            graphicsPreload.test_and_set();
            graphicsPreload.notify_all();
            physicsPreload.test_and_set();
            physicsPreload.notify_all();
            StopThread(false);
        }
        {
            std::lock_guard lock(actionMutex);
            actionQueue.clear();
        }
        StopThread(true);

        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

        for (auto &list : scenes) {
            list.clear();
        }
        stagedScenes.DropAll([&stagingLock, &liveLock](std::shared_ptr<Scene> &scene) {
            scene->RemoveScene(stagingLock, liveLock);
            scene.reset();
        });
        if (playerScene) {
            playerScene->RemoveScene(stagingLock, liveLock);
            playerScene.reset();
        }
        if (bindingsScene) {
            bindingsScene->RemoveScene(stagingLock, liveLock);
            bindingsScene.reset();
        }
    }

    void SceneManager::RunSceneActions() {
        std::unique_lock lock(actionMutex);
        while (!actionQueue.empty()) {
            auto item = std::move(actionQueue.front());
            actionQueue.pop_front();
            lock.unlock();

            if (item.action == SceneAction::ApplySystemScene) {
                ZoneScopedN("ApplySystemScene");
                ZoneStr(item.sceneName);
                if (!item.applyCallback) {
                    // Load the System scene from json
                    AddScene(item.sceneName, SceneType::System);
                } else {
                    auto scene = stagedScenes.Load(item.sceneName);
                    if (!scene) {
                        scene = std::make_shared<Scene>(item.sceneName, SceneType::System);
                        stagedScenes.Register(item.sceneName, scene);
                        scenes[SceneType::System].emplace_back(scene);
                    }

                    {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        item.applyCallback(stagingLock, scene);
                    }
                    {
                        Tracef("Applying system scene: %s", scene->name);
                        auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                        scene->ApplyScene(stagingLock, liveLock);
                    }
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::EditStagingScene) {
                ZoneScopedN("EditStagingScene");
                ZoneStr(item.sceneName);
                if (item.applyCallback) {
                    auto scene = stagedScenes.Load(item.sceneName);
                    if (scene) {
                        if (scene->type != SceneType::System) {
                            Debugf("Editing staging scene: %s", scene->name);
                            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                            item.applyCallback(stagingLock, scene);
                        } else {
                            Errorf("SceneManager::EditStagingScene: Cannot edit system scene: %s", scene->name);
                        }
                    } else {
                        Errorf("SceneManager::EditStagingScene: scene %s not found", item.sceneName);
                    }
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::EditLiveECS) {
                ZoneScopedN("EditLiveECS");
                if (item.editCallback) {
                    auto liveLock = ecs::StartTransaction<ecs::WriteAll>();
                    item.editCallback(liveLock);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ApplyScene) {
                ZoneScopedN("ApplyScene");
                ZoneStr(item.sceneName);
                auto scene = stagedScenes.Load(item.sceneName);
                if (scene) {
                    Debugf("Applying scene: %s", scene->name);
                    auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                    auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                    scene->ApplyScene(stagingLock, liveLock);
                } else {
                    Errorf("SceneManager::ApplyScene: scene %s not found", item.sceneName);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::LoadScene) {
                ZoneScopedN("LoadScene");
                ZoneStr(item.sceneName);
                // Unload all current scenes first
                size_t expectedCount = scenes[SceneType::Async].size() + scenes[SceneType::World].size();
                if (expectedCount > 0) {
                    auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                    auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                    scenes[SceneType::Async].clear();
                    scenes[SceneType::World].clear();
                    size_t removedCount = stagedScenes.DropAll(
                        [&stagingLock, &liveLock](std::shared_ptr<Scene> &scene) {
                            scene->RemoveScene(stagingLock, liveLock);
                            scene.reset();
                        });
                    Assertf(removedCount >= expectedCount,
                        "Expected to remove %u scenes, got %u",
                        expectedCount,
                        removedCount);
                }

                AddScene(item.sceneName, SceneType::World, [this](auto stagingLock, auto liveLock, auto scene) {
                    RespawnPlayer(liveLock, player);
                });
                item.promise.set_value();
            } else if (item.action == SceneAction::ReloadScene) {
                ZoneScopedN("ReloadScene");
                ZoneStr(item.sceneName);
                if (item.sceneName.empty()) {
                    // Reload all async and user scenes
                    std::vector<std::pair<std::string, SceneType>> reloadScenes;
                    size_t reloadCount = scenes[SceneType::Async].size() + scenes[SceneType::World].size();
                    reloadScenes.reserve(reloadCount);

                    if (reloadCount > 0) {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                        for (auto &scene : scenes[SceneType::World]) {
                            reloadScenes.emplace_back(scene->name, SceneType::World);
                        }
                        for (auto &scene : scenes[SceneType::Async]) {
                            reloadScenes.emplace_back(scene->name, SceneType::Async);
                        }
                        scenes[SceneType::World].clear();
                        scenes[SceneType::Async].clear();

                        size_t removedCount = stagedScenes.DropAll(
                            [&stagingLock, &liveLock](std::shared_ptr<Scene> &scene) {
                                scene->RemoveScene(stagingLock, liveLock);
                                scene.reset();
                            });
                        Assertf(removedCount >= reloadScenes.size(),
                            "Expected to remove %u scenes, got %u",
                            reloadScenes.size(),
                            removedCount);
                    }

                    for (auto &[name, type] : reloadScenes) {
                        AddScene(name, type);
                    }
                } else {
                    auto loadedScene = stagedScenes.Load(item.sceneName);
                    if (loadedScene != nullptr) {
                        auto sceneType = loadedScene->type;
                        auto &sceneList = scenes[sceneType];
                        sceneList.erase(std::remove(sceneList.begin(), sceneList.end(), loadedScene));

                        {
                            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                            loadedScene->RemoveScene(stagingLock, liveLock);
                        }

                        loadedScene.reset();
                        Assert(stagedScenes.Drop(item.sceneName), "Staged scene still in use after removal");

                        AddScene(item.sceneName, sceneType);
                    } else {
                        Errorf("Scene not current loaded: %s", item.sceneName);
                    }
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::AddScene) {
                ZoneScopedN("AddScene");
                ZoneStr(item.sceneName);
                AddScene(item.sceneName, SceneType::World);
                item.promise.set_value();
            } else if (item.action == SceneAction::RemoveScene) {
                ZoneScopedN("RemoveScene");
                ZoneStr(item.sceneName);
                auto loadedScene = stagedScenes.Load(item.sceneName);
                if (loadedScene != nullptr) {
                    auto &sceneList = scenes[loadedScene->type];
                    sceneList.erase(std::remove(sceneList.begin(), sceneList.end(), loadedScene));

                    {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                        loadedScene->RemoveScene(stagingLock, liveLock);
                    }

                    loadedScene.reset();
                    Assert(stagedScenes.Drop(item.sceneName), "Staged scene still in use after removal");
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ReloadPlayer) {
                ZoneScopedN("ReloadPlayer");
                if (playerScene) {
                    auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                    auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                    playerScene->RemoveScene(stagingLock, liveLock);
                    playerScene.reset();
                    player = ecs::Entity();
                }

                playerScene = LoadSceneJson("player", SceneType::World, ecs::SceneInfo::Priority::Player);
                if (playerScene) {
                    PreloadAndApplyScene(playerScene, [this](auto stagingLock, auto liveLock, auto scene) {
                        auto stagingPlayer = scene->GetStagingEntity(entities::Player.Name());
                        if (stagingPlayer.template Has<ecs::SceneInfo>(stagingLock)) {
                            auto &sceneInfo = stagingPlayer.template Get<ecs::SceneInfo>(stagingLock);
                            player = sceneInfo.liveId;
                        }
                        Assert(!!player, "Player scene doesn't contain an entity named player");
                        RespawnPlayer(liveLock, player);
                    });
                } else {
                    Errorf("Failed to load player scene!");
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ReloadBindings) {
                ZoneScopedN("ReloadBindings");
                if (bindingsScene) {
                    auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                    auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                    bindingsScene->RemoveScene(stagingLock, liveLock);
                    bindingsScene.reset();
                }

                bindingsScene = LoadBindingsJson();
                if (bindingsScene) {
                    auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
                    auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                    bindingsScene->ApplyScene(stagingLock, liveLock);
                } else {
                    Errorf("Failed to load bindings scene!");
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::SyncScene) {
                ZoneScopedN("SyncScene");
                UpdateSceneConnections();
                item.promise.set_value();
            } else {
                Abortf("Unsupported SceneAction: %s", item.action);
            }

            lock.lock();
        }
    }

    void SceneManager::UpdateSceneConnections() {
        ZoneScoped;
        requiredSceneList.clear();
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::SceneConnection>>();

            for (auto &ent : lock.EntitiesWith<ecs::SceneConnection>()) {
                auto loadSignal = ecs::SignalBindings::GetSignal(lock, ent, "load_scene_connection");
                if (loadSignal >= 0.5) {
                    auto &connection = ent.Get<ecs::SceneConnection>(lock);
                    for (auto &sceneName : connection.scenes) {
                        if (!contains(requiredSceneList, sceneName)) requiredSceneList.emplace_back(sceneName);
                    }
                }
            }
        }

        scenes[SceneType::Async].clear();
        for (auto &sceneName : requiredSceneList) {
            auto loadedScene = stagedScenes.Load(sceneName);
            if (loadedScene) {
                if (loadedScene->type == SceneType::Async) scenes[SceneType::Async].push_back(loadedScene);
            } else {
                AddScene(sceneName, SceneType::Async);
            }
        }
    }

    void SceneManager::Frame() {
        RunSceneActions();
        UpdateSceneConnections();

        stagedScenes.Tick(this->interval, [this](std::shared_ptr<Scene> &scene) {
            ZoneScopedN("RemoveExpiredScene");
            ZoneStr(scene->name);
            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
            scene->RemoveScene(stagingLock, liveLock);
            scene.reset();
        });
        ecs::GetEntityRefs().Tick(this->interval);
    }

    void SceneManager::QueueAction(SceneAction action, std::string sceneName, PreApplySceneCallback callback) {
        std::lock_guard lock(actionMutex);
        if (exiting.load()) return;
        actionQueue.emplace_back(action, sceneName, callback);
    }

    void SceneManager::QueueAction(SceneAction action, EditSceneCallback callback) {
        std::lock_guard lock(actionMutex);
        if (exiting.load()) return;
        actionQueue.emplace_back(action, callback);
    }

    void SceneManager::QueueActionAndBlock(SceneAction action, std::string sceneName, PreApplySceneCallback callback) {
        std::future<void> future;
        {
            std::lock_guard lock(actionMutex);
            if (exiting.load()) return;
            auto &entry = actionQueue.emplace_back(action, sceneName, callback);
            future = entry.promise.get_future();
        }
        try {
            future.get();
        } catch (const std::future_error &) {
            Abortf("SceneManager action did not complete: %s(%s)", action, sceneName);
        }
    }

    void SceneManager::PreloadSceneGraphics(ScenePreloadCallback callback) {
        std::shared_lock lock(preloadMutex);
        if (preloadScene) {
            auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
            if (callback(stagingLock, preloadScene)) {
                graphicsPreload.test_and_set();
                graphicsPreload.notify_all();
            }
        }
    }

    void SceneManager::PreloadScenePhysics(ScenePreloadCallback callback) {
        std::shared_lock lock(preloadMutex);
        if (preloadScene) {
            auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
            if (callback(stagingLock, preloadScene)) {
                physicsPreload.test_and_set();
                physicsPreload.notify_all();
            }
        }
    }

    void SceneManager::PreloadAndApplyScene(const std::shared_ptr<Scene> &scene, OnApplySceneCallback callback) {
        ZoneScopedN("ScenePreload");
        ZoneStr(scene->name);
        {
            std::lock_guard lock(preloadMutex);
            Assertf(!preloadScene, "Already preloading %s when trying to preload %s", preloadScene->name, scene->name);
            if (exiting.load()) return;
            preloadScene = scene;
            graphicsPreload.clear();
            physicsPreload.clear();
        }

        if (!skipPreload) {
            while (!graphicsPreload.test()) {
                graphicsPreload.wait(false);
            }
            while (!physicsPreload.test()) {
                physicsPreload.wait(false);
            }
        }

        {
            Tracef("Applying scene: %s", scene->name);
            auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

            scene->ApplyScene(stagingLock, liveLock);

            if (callback) callback(stagingLock, liveLock, scene);

            {
                std::lock_guard lock(preloadMutex);
                preloadScene.reset();
            }
        }
    }

    std::shared_ptr<Scene> SceneManager::LoadSceneJson(const std::string &sceneName,
        SceneType sceneType,
        ecs::SceneInfo::Priority priority) {
        Logf("Loading scene: %s", sceneName);

        auto asset = Assets().Load("scenes/" + sceneName + ".json", AssetType::Bundled, true)->Get();
        if (!asset) {
            Errorf("Scene not found: %s", sceneName);
            return nullptr;
        }

        picojson::value root;
        string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("Failed to parse scene (%s): %s", sceneName, err);
            return nullptr;
        }
        if (!root.is<picojson::object>()) {
            Errorf("Failed to parse scene (%s): %s", sceneName, root.to_str());
            return nullptr;
        }
        auto &sceneObj = root.get<picojson::object>();

        auto scene = make_shared<Scene>(sceneName, sceneType, asset);
        ecs::EntityScope scope;
        scope.scene = scene;
        scope.prefix.scene = scene->name;

        if (sceneObj.count("priority")) {
            json::Load(scope, priority, sceneObj["priority"]);
        }

        if (sceneObj.count("properties")) {
            scene->properties = make_shared<ecs::SceneProperties>();
            auto &properties = *scene->properties;
            auto &propertiesValue = sceneObj["properties"];
            if (propertiesValue.is<picojson::object>()) {
                for (auto &property : propertiesValue.get<picojson::object>()) {
                    if (property.first == "gravity") {
                        json::Load(scope, properties.fixedGravity, property.second);
                    } else if (property.first == "gravity_func") {
                        if (property.second.is<std::string>()) {
                            auto gravityFunc = property.second.get<std::string>();
                            if (gravityFunc == "station_spin") {
                                properties.gravityFunction = [](glm::vec3 position) {
                                    position.x = 0;
                                    // Derived from centripetal acceleration formula, rotating around the origin
                                    const float spinRpm = 2.42f; // Calculated for ~1G at 153m radius
                                    float spinTerm = M_PI * spinRpm / 30;
                                    return spinTerm * spinTerm * position;
                                };
                            }
                        }
                    }
                }
            } else {
                Errorf("Scene contains invalid properties (%s): %s", sceneName, propertiesValue.to_str());
            }
        }

        if (sceneObj.count("entities")) {
            auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();

            auto &entityList = sceneObj["entities"];
            std::vector<ecs::Entity> entities;
            for (auto &value : entityList.get<picojson::array>()) {
                auto ent = value.get<picojson::object>();

                bool hasName = ent.count("name") && ent["name"].is<string>();
                auto relativeName = hasName ? ent["name"].get<string>() : "";
                ecs::Entity entity = scene->NewRootEntity(lock, scene, priority, relativeName);

                for (auto comp : ent) {
                    if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                    auto componentType = ecs::LookupComponent(comp.first);
                    if (componentType != nullptr) {
                        if (!componentType->LoadEntity(lock, scope, entity, comp.second)) {
                            Errorf("Failed to load component, ignoring: %s", comp.first);
                        }
                    } else {
                        Errorf("Unknown component, ignoring: %s", comp.first);
                    }
                }

                entities.emplace_back(entity);
            }

            for (auto &e : entities) {
                if (e.Has<ecs::Script>(lock)) {
                    e.Get<ecs::Script>(lock).Prefab(lock, e);
                }
            }
        }
        return scene;
    }

    std::shared_ptr<Scene> SceneManager::LoadBindingsJson() {
        Logf("Loading bindings json: %s", InputBindingConfigPath);

        std::shared_ptr<const Asset> bindingConfig;
        if (!std::filesystem::exists(InputBindingConfigPath)) {
            bindingConfig = Assets().Load("default_input_bindings.json", AssetType::Bundled, true)->Get();
            Assert(bindingConfig, "Default input binding config missing");

            // TODO: Create CFunc to save current input bindings to file
            // std::ofstream file(InputBindingConfigPath, std::ios::binary);
            // Assertf(file.is_open(), "Failed to create binding config file: %s", InputBindingConfigPath);
            // file << bindingConfig->String();
            // file.close();
        } else {
            bindingConfig = Assets().Load(InputBindingConfigPath, AssetType::External, true)->Get();
            Assertf(bindingConfig, "Failed to load input binding config: %s", InputBindingConfigPath);
        }

        auto scene = make_shared<Scene>("bindings", SceneType::System);
        ecs::EntityScope scope;
        scope.scene = scene;
        scope.prefix.scene = scene->name;

        picojson::value root;
        string err = picojson::parse(root, bindingConfig->String());
        if (!err.empty()) Abortf("Failed to parse input binding json file: %s", err);
        if (!root.is<picojson::object>()) {
            Abortf("Failed to parse input binding json: %s", root.to_str());
        }

        {
            auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();

            for (auto &param : root.get<picojson::object>()) {
                Tracef("Loading input for: %s", param.first);
                if (param.first.find(':') == std::string::npos) {
                    Abortf("Binding entity does not have scene name: %s", param.first);
                }
                auto entity = scene->NewRootEntity(lock, scene, ecs::SceneInfo::Priority::Bindings, param.first);
                if (entity) {
                    for (auto &comp : param.second.get<picojson::object>()) {
                        if (comp.first[0] == '_') continue;

                        auto componentType = ecs::LookupComponent(comp.first);
                        if (componentType != nullptr) {
                            bool result = componentType->LoadEntity(lock, scope, entity, comp.second);
                            Assertf(result, "Failed to load component type: %s", comp.first);
                        } else {
                            Errorf("Unknown component, ignoring: %s", comp.first);
                        }
                    }
                } else {
                    Errorf("Invalid binding entity name: %s", param.first);
                }
            }
        }
        return scene;
    }

    void SceneManager::TranslateSceneByConnection(const std::shared_ptr<Scene> &scene) {
        auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo, ecs::Animation>,
            ecs::Write<ecs::TransformTree, ecs::Animation>>();
        auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::TransformSnapshot>>();

        ecs::Entity liveConnection, stagingConnection;
        for (auto &e : stagingLock.EntitiesWith<ecs::SceneConnection>()) {
            if (!e.Has<ecs::SceneConnection, ecs::SceneInfo, ecs::Name>(stagingLock)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
            if (sceneInfo.scene.lock() != scene) continue;

            auto &name = e.Get<const ecs::Name>(stagingLock);
            liveConnection = ecs::EntityRef(name).Get(liveLock);
            if (liveConnection.Has<ecs::SceneConnection, ecs::TransformSnapshot>(liveLock)) {
                stagingConnection = e;
                break;
            }
        }
        if (stagingConnection.Has<ecs::TransformTree>(stagingLock) &&
            liveConnection.Has<ecs::TransformSnapshot>(liveLock)) {
            auto &liveTransform = liveConnection.Get<const ecs::TransformSnapshot>(liveLock);
            auto stagingTransform =
                stagingConnection.Get<const ecs::TransformTree>(stagingLock).GetGlobalTransform(stagingLock);
            glm::quat deltaRotation = liveTransform.GetRotation() * glm::inverse(stagingTransform.GetRotation());
            glm::vec3 deltaPos = liveTransform.GetPosition() - deltaRotation * stagingTransform.GetPosition();

            for (auto &e : stagingLock.EntitiesWith<ecs::TransformTree>()) {
                if (!e.Has<ecs::TransformTree, ecs::SceneInfo>(stagingLock)) continue;
                auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
                if (sceneInfo.scene.lock() != scene) continue;

                auto &transform = e.Get<ecs::TransformTree>(stagingLock);
                if (!transform.parent) {
                    transform.pose.SetPosition(deltaRotation * transform.pose.GetPosition() + deltaPos);
                    transform.pose.SetRotation(deltaRotation * transform.pose.GetRotation());

                    if (e.Has<ecs::Animation>(stagingLock)) {
                        auto &animation = e.Get<ecs::Animation>(stagingLock);
                        for (auto &state : animation.states) {
                            state.pos = deltaRotation * state.pos + deltaPos;
                        }
                    }
                }
            }
        }
    }

    std::shared_ptr<Scene> SceneManager::AddScene(std::string sceneName,
        SceneType sceneType,
        OnApplySceneCallback callback) {
        ZoneScoped;
        ZonePrintf("%s scene: %s", sceneType, sceneName);
        auto loadedScene = stagedScenes.Load(sceneName);
        if (loadedScene != nullptr) {
            Logf("Scene %s already loaded", sceneName);
            return loadedScene;
        }

        loadedScene = LoadSceneJson(sceneName, sceneType, ecs::SceneInfo::Priority::Scene);
        if (loadedScene) {
            TranslateSceneByConnection(loadedScene);
            PreloadAndApplyScene(loadedScene, callback);

            stagedScenes.Register(sceneName, loadedScene);
            scenes[sceneType].emplace_back(loadedScene);
        } else {
            Errorf("Failed to load scene: %s", sceneName);
        }
        return loadedScene;
    }

    void SceneManager::RespawnPlayer(
        ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>> lock,
        ecs::Entity player) {
        auto spawn = entities::Spawn.Get(lock);
        if (!spawn.Has<ecs::TransformSnapshot>(lock)) return;

        auto spawnTransform = spawn.Get<const ecs::TransformSnapshot>(lock);
        spawnTransform.SetScale(glm::vec3(1));

        if (player.Has<ecs::TransformSnapshot, ecs::TransformTree>(lock)) {
            auto &playerTransform = player.Get<ecs::TransformSnapshot>(lock);
            auto &playerTree = player.Get<ecs::TransformTree>(lock);
            Assert(!playerTree.parent, "Player entity should not have a TransformTree parent");
            playerTransform = spawnTransform;
            playerTree.pose = spawnTransform;
        }
        auto vrOrigin = entities::VrOrigin.Get(lock);
        if (vrOrigin.Has<ecs::TransformSnapshot, ecs::TransformTree>(lock)) {
            auto &vrTransform = vrOrigin.Get<ecs::TransformSnapshot>(lock);
            auto &vrTree = vrOrigin.Get<ecs::TransformTree>(lock);
            Assert(!vrTree.parent, "VR Origin entity should not have a TransformTree parent");
            vrTransform = spawnTransform;
            vrTree.pose = spawnTransform;
        }
    }

    void SceneManager::PrintScene(std::string filterName) {
        to_lower(filterName);

        {
            auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
            auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();

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
                                Assert(stagingScene, "Missing SceneInfo scene on player entity");
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
                                Assert(stagingScene, "Missing SceneInfo scene on binding entity");
                                Logf("  -> %s scene", stagingScene->name);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }

            for (size_t sceneTypeI = 0; sceneTypeI < scenes.size(); sceneTypeI++) {
                auto sceneType = static_cast<SceneType>(sceneTypeI);
                std::string typeName(magic_enum::enum_name<SceneType>(sceneType));
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
                                        Assert(stagingScene, "Missing SceneInfo scene on entity");
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
