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
#include "game/SceneProperties.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <picojson/picojson.h>
#include <robin_hood.h>
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

    static std::atomic<std::thread::id> activeSceneManagerThread;

    std::shared_ptr<Scene> SceneRef::Lock() const {
        Assertf(std::this_thread::get_id() == activeSceneManagerThread,
            "SceneRef::Lock() must only be called in SceneManaager thread");
        auto result = ptr.lock();
        Assertf(result, "SceneRef points to null scene: %s (%s)", name, type);
        return result;
    }

    SceneManager::SceneManager(bool skipPreload) : RegisteredThread("SceneManager", 30.0), skipPreload(skipPreload) {
        activeSceneManagerThread = std::this_thread::get_id();
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
        funcs.Register("respawn", "Respawn the player", [this]() {
            QueueActionAndBlock(SceneAction::RespawnPlayer);
        });
        funcs.Register("reloadplayer", "Reload player scene", [this]() {
            QueueActionAndBlock(SceneAction::ReloadPlayer);
        });
        funcs.Register("reloadbindings", "Reload input bindings", [this]() {
            QueueActionAndBlock(SceneAction::ReloadBindings);
        });
        funcs.Register(this, "printscene", "Print info about currently loaded scenes", &SceneManager::PrintScene);

        StartThread();
    }

    SceneManager::~SceneManager() {
        Shutdown();
    }

    void SceneManager::Shutdown() {
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
        activeSceneManagerThread = std::this_thread::get_id();

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

        LogOnExit logOnExit = "SceneManager shut down ================================================";
    }

    bool SceneManager::ThreadInit() {
        activeSceneManagerThread = std::this_thread::get_id();
        return true;
    }

    std::vector<SceneRef> SceneManager::GetActiveScenes() {
        std::lock_guard lock(activeSceneMutex);
        return activeSceneCache;
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
                if (!item.editSceneCallback) {
                    // Load the System scene from json
                    AddScene(item.sceneName, SceneType::System);
                } else {
                    auto scene = stagedScenes.Load(item.sceneName);
                    if (!scene) {
                        scene = std::make_shared<Scene>(item.sceneName, SceneType::System, ecs::ScenePriority::System);
                        stagedScenes.Register(item.sceneName, scene);
                        scenes[SceneType::System].emplace_back(scene);
                    }

                    {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        item.editSceneCallback(stagingLock, scene);
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
                if (item.editSceneCallback) {
                    auto scene = stagedScenes.Load(item.sceneName);
                    if (scene) {
                        if (scene->type != SceneType::System) {
                            Tracef("Editing staging scene: %s", scene->name);
                            {
                                auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                                item.editSceneCallback(stagingLock, scene);
                            }
                        } else {
                            Errorf("SceneManager::EditStagingScene: Cannot edit system scene: %s", scene->name);
                        }
                    } else {
                        Errorf("SceneManager::EditStagingScene: scene %s not found", item.sceneName);
                    }
                } else {
                    Errorf("SceneManager::EditStagingScene called on %s without editSceneCallback", item.sceneName);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::RefreshScenePrefabs) {
                ZoneScopedN("RefreshScenePrefabs");
                ZoneStr(item.sceneName);
                auto scene = stagedScenes.Load(item.sceneName);
                if (scene) {
                    RefreshPrefabs(scene);
                } else {
                    Errorf("SceneManager::RefreshScenePrefabs: scene %s not found", item.sceneName);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ApplyStagingScene) {
                ZoneScopedN("ApplyStagingScene");
                ZoneStr(item.sceneName);
                auto scene = stagedScenes.Load(item.sceneName);
                if (scene) {
                    if (scene->type != SceneType::System) {
                        Tracef("Applying staging scene: %s", scene->name);
                        PreloadAndApplyScene(scene, true);
                    } else {
                        Errorf("SceneManager::ApplyStagingScene: Cannot apply system scene: %s", scene->name);
                    }
                } else {
                    Errorf("SceneManager::ApplyStagingScene: scene %s not found", item.sceneName);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::SaveStagingScene) {
                ZoneScopedN("SaveStagingScene");
                ZoneStr(item.sceneName);
                SaveSceneJson(item.sceneName);
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
                    RespawnPlayer(liveLock);
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
            } else if (item.action == SceneAction::RespawnPlayer) {
                ZoneScopedN("RespawnPlayer");
                {
                    auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name>,
                        ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>>();
                    RespawnPlayer(liveLock);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ReloadPlayer) {
                ZoneScopedN("ReloadPlayer");
                if (playerScene) {
                    auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                    auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                    playerScene->RemoveScene(stagingLock, liveLock);
                    playerScene.reset();
                }

                playerScene = LoadSceneJson("player", SceneType::World);
                if (playerScene) {
                    PreloadAndApplyScene(playerScene, false, [this](auto stagingLock, auto liveLock, auto scene) {
                        ecs::Entity stagingPlayer = scene->GetStagingEntity(entities::Player.Name());
                        Assert(stagingPlayer.Has<ecs::SceneInfo>(stagingLock),
                            "Player scene doesn't contain an entity named player");

                        RespawnPlayer(liveLock);
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
            } else if (item.action == SceneAction::RunCallback) {
                ZoneScopedN("RunCallback");
                if (item.voidCallback) item.voidCallback();
                item.promise.set_value();
            } else {
                Abortf("Unsupported SceneAction: %s", item.action);
            }

            lock.lock();
        }
    }

    void SceneManager::UpdateSceneConnections() {
        ZoneScoped;
        robin_hood::unordered_set<std::string> requiredSceneList = {};
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::SceneConnection>>();

            for (auto &ent : lock.EntitiesWith<ecs::SceneConnection>()) {
                auto &connection = ent.Get<ecs::SceneConnection>(lock);
                for (auto &[sceneName, signals] : connection.scenes) {
                    for (auto &expr : signals) {
                        if (expr.Evaluate(lock) >= 0.5) {
                            requiredSceneList.emplace(sceneName);
                            break;
                        }
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

        {
            // Update active scene list
            std::lock_guard lock(activeSceneMutex);
            activeSceneCache.clear();
            for (auto &sceneList : scenes) {
                for (auto &scene : sceneList) {
                    activeSceneCache.emplace_back(scene);
                }
            }
        }

        stagedScenes.Tick(this->interval, [](std::shared_ptr<Scene> &scene) {
            ZoneScopedN("RemoveExpiredScene");
            ZoneStr(scene->name);
            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
            scene->RemoveScene(stagingLock, liveLock);
            scene.reset();
        });
        ecs::GetEntityRefs().Tick(this->interval);
    }

    void SceneManager::QueueAction(SceneAction action, std::string sceneName, EditSceneCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(action, sceneName, callback);
    }

    void SceneManager::QueueAction(SceneAction action, EditCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(action, callback);
    }

    void SceneManager::QueueAction(SceneAction action, VoidCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(action, callback);
    }

    void SceneManager::QueueActionAndBlock(SceneAction action, std::string sceneName, EditSceneCallback callback) {
        std::future<void> future;
        {
            std::lock_guard lock(actionMutex);
            if (state != ThreadState::Started) return;
            auto &entry = actionQueue.emplace_back(action, sceneName, callback);
            future = entry.promise.get_future();
        }
        try {
            future.get();
        } catch (const std::future_error &) {
            Abortf("SceneManager action did not complete: %s(%s)", action, sceneName);
        }
    }

    void SceneManager::QueueActionAndBlock(SceneAction action, VoidCallback callback) {
        std::future<void> future;
        {
            std::lock_guard lock(actionMutex);
            if (state != ThreadState::Started) return;
            auto &entry = actionQueue.emplace_back(action, callback);
            future = entry.promise.get_future();
        }
        try {
            future.get();
        } catch (const std::future_error &) {
            Abortf("SceneManager action did not complete: %s", action);
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

    void SceneManager::PreloadAndApplyScene(const std::shared_ptr<Scene> &scene,
        bool resetLive,
        OnApplySceneCallback callback) {
        ZoneScopedN("ScenePreload");
        ZoneStr(scene->name);
        {
            std::lock_guard lock(preloadMutex);
            Assertf(!preloadScene, "Already preloading %s when trying to preload %s", preloadScene->name, scene->name);
            if (state != ThreadState::Started) return;
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

            scene->ApplyScene(stagingLock, liveLock, resetLive);

            if (callback) callback(stagingLock, liveLock, scene);

            {
                std::lock_guard lock(preloadMutex);
                preloadScene.reset();
            }
        }
    }

    void SceneManager::RefreshPrefabs(const std::shared_ptr<Scene> &scene) {
        ZoneScopedN("RefreshPrefabs");
        ZoneStr(scene->name);
        {
            Tracef("Refreshing scene prefabs: %s", scene->name);
            auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();

            // Remove all entities generated by a prefab
            for (auto &e : lock.EntitiesWith<ecs::SceneInfo>()) {
                auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                if (sceneInfo.scene != scene) continue;

                if (sceneInfo.prefabStagingId) scene->RemovePrefabEntity(lock, e);
            }
            for (auto &e : lock.EntitiesWith<ecs::Scripts>()) {
                if (!e.Has<ecs::Scripts, ecs::SceneInfo>(lock)) continue;
                auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                if (sceneInfo.scene != scene) continue;
                if (sceneInfo.prefabStagingId) continue;

                ecs::Scripts::RunPrefabs(lock, e);
            }
        }
    }

    std::shared_ptr<Scene> SceneManager::LoadSceneJson(const std::string &sceneName, SceneType sceneType) {
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

        auto scene = make_shared<Scene>(sceneName,
            sceneType,
            sceneType == SceneType::System ? ecs::ScenePriority::System : ecs::ScenePriority::Scene,
            asset);

        ecs::EntityScope scope(scene->name, "");
        if (sceneObj.count("priority")) {
            json::Load(scope, scene->priority, sceneObj["priority"]);
        }

        if (sceneObj.count("properties")) {
            scene->properties = make_shared<SceneProperties>();
            auto &properties = *scene->properties;
            if (!json::Load(scope, properties, sceneObj["properties"])) {
                Errorf("Scene contains invalid properties: %s", sceneName);
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
                ecs::Entity entity = scene->NewRootEntity(lock, scene, relativeName);

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
                if (e.Has<ecs::Scripts>(lock)) {
                    ecs::Scripts::RunPrefabs(lock, e);
                }
            }
        }
        return scene;
    }

    void SceneManager::SaveSceneJson(const std::string &sceneName) {
        auto scene = stagedScenes.Load(sceneName);
        if (scene) {
            Tracef("Saving staging scene: %s", scene->name);
            auto staging = ecs::StartStagingTransaction<ecs::ReadAll>();

            ecs::EntityScope scope(scene->name, "");

            picojson::array entities;
            for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
                if (!e.Has<ecs::SceneInfo>(staging)) continue;
                auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
                // Skip entities that aren't part of this scene, or were created by a prefab script
                if (sceneInfo.scene != scene || sceneInfo.prefabStagingId) continue;

                static auto componentOrderFunc = [](const std::string &a, const std::string &b) {
                    // Sort component keys in the order they are defined in the ECS
                    return ecs::GetComponentIndex(a) < ecs::GetComponentIndex(b);
                };
                picojson::object components(componentOrderFunc);
                if (e.Has<ecs::Name>(staging)) {
                    auto &name = e.Get<ecs::Name>(staging);
                    if (scene->ValidateEntityName(name)) {
                        json::Save(scope, components["name"], name);
                    }
                }
                ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                    if (comp.HasComponent(staging, e)) {
                        if (comp.metadata.fields.empty()) {
                            components[comp.name].set<picojson::object>({});
                        }
                        comp.SaveEntity(staging, scope, components[comp.name], e);
                    }
                });
                entities.emplace_back(components);
            }

            static auto sceneOrderFunc = [](const std::string &a, const std::string &b) {
                // Force "entities" to be sorted last
                if (b == "entities") {
                    return a < "zentities";
                } else if (a == "entities") {
                    return "zentities" < b;
                } else {
                    return a < b;
                }
            };
            picojson::object sceneObj(sceneOrderFunc);
            if (scene->properties) {
                static const SceneProperties defaultProperties = {};
                json::SaveIfChanged(scope, sceneObj, "properties", *scene->properties, defaultProperties);
            }
            json::SaveIfChanged(scope, sceneObj, "priority", scene->priority, ecs::ScenePriority::Scene);
            sceneObj["entities"] = picojson::value(entities);
            auto val = picojson::value(sceneObj);
            auto scenePath = scene->asset->path;
            Logf("Saving scene %s to '%s'", scene->name, scenePath.string());

            std::ofstream out;
            if (Assets().OutputStream(scenePath.string(), out)) {
                auto outputJson = val.serialize(true);
                out.write(outputJson.c_str(), outputJson.size());
                out.close();
            }
        } else {
            Errorf("SceneManager::SaveSceneJson: scene %s not found", sceneName);
        }
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

        auto scene = make_shared<Scene>("bindings", SceneType::System, ecs::ScenePriority::Bindings);
        ecs::EntityScope scope(scene->name, "");

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
                auto entity = scene->NewRootEntity(lock, scene, param.first);
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

        loadedScene = LoadSceneJson(sceneName, sceneType);
        if (loadedScene) {
            loadedScene->UpdateRootTransform();
            PreloadAndApplyScene(loadedScene, false, callback);

            stagedScenes.Register(sceneName, loadedScene);
            scenes[sceneType].emplace_back(loadedScene);
        } else {
            Errorf("Failed to load scene: %s", sceneName);
        }
        return loadedScene;
    }

    void SceneManager::RespawnPlayer(
        ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>> lock) {
        auto spawn = entities::Spawn.Get(lock);
        if (!spawn.Has<ecs::TransformSnapshot>(lock)) {
            if (!scenes[SceneType::Async].empty() || !scenes[SceneType::World].empty()) {
                // If no scenes are loaded, this is expected. (Player is the first scene to load on boot)
                Errorf("RespawnPlayer: Spawn point entity missing: %s", entities::Spawn.Name().String());
            }
            return;
        }

        auto spawnTransform = spawn.Get<const ecs::TransformSnapshot>(lock);
        spawnTransform.SetScale(glm::vec3(1));

        auto player = entities::Player.Get(lock);
        if (player.Has<ecs::TransformSnapshot, ecs::TransformTree>(lock)) {
            auto &playerTransform = player.Get<ecs::TransformSnapshot>(lock);
            auto &playerTree = player.Get<ecs::TransformTree>(lock);
            Assert(!playerTree.parent, "Player entity should not have a TransformTree parent");
            playerTransform = spawnTransform;
            playerTree.pose = spawnTransform;
        } else {
            if (!player.Exists(lock)) {
                Errorf("RespawnPlayer: Player entity missing: %s", entities::Player.Name().String());
            } else {
                Errorf("RespawnPlayer: Player entity does not have a Transform: %s", entities::Player.Name().String());
            }
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
                    if (!e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) continue;
                    auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                    if (sceneInfo.scene != playerScene) continue;

                    Logf("  %s", ecs::ToString(liveLock, e));
                    auto stagingId = sceneInfo.nextStagingId;
                    while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                        auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                        Assert(stagingInfo.scene, "Missing SceneInfo scene on player entity");
                        Logf("  -> %s scene", stagingInfo.scene.name);
                        stagingId = stagingInfo.nextStagingId;
                    }
                }
            }

            if (filterName.empty() || filterName == "bindings") {
                Logf("Binding scene entities:");
                for (auto &e : liveLock.EntitiesWith<ecs::Name>()) {
                    if (!e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) continue;
                    auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                    if (sceneInfo.scene != bindingsScene) continue;

                    Logf("  %s", ecs::ToString(liveLock, e));
                    auto stagingId = sceneInfo.nextStagingId;
                    while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                        auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                        Assert(stagingInfo.scene, "Missing SceneInfo scene on binding entity");
                        Logf("  -> %s scene", stagingInfo.scene.name);
                        stagingId = stagingInfo.nextStagingId;
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
                            if (!e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) continue;
                            auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                            if (sceneInfo.scene != scene) continue;

                            Logf("  %s", ecs::ToString(liveLock, e));
                            auto stagingId = sceneInfo.nextStagingId;
                            while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                Assert(stagingInfo.scene, "Missing SceneInfo scene on entity");
                                Logf(" -> %s scene (%s type)", stagingInfo.scene.name, stagingInfo.scene.type);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace sp
