/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SceneManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "common/Logging.hh"
#include "common/Tracing.hh"
#include "console/Console.hh"
#include "console/ConsoleBindingManager.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/SignalManager.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <picojson.h>
#include <robin_hood.h>
#include <shared_mutex>

namespace sp {
    SceneManager &GetSceneManager() {
        // Ensure ECS, ScriptManager, and AssetManager are constructed first so they are destructed in the right order.
        ecs::World();
        ecs::StagingWorld();
        ecs::GetScriptManager();
        sp::Assets();
        static SceneManager sceneManager;
        return sceneManager;
    }

    static std::atomic<std::thread::id> activeSceneManagerThread;

    std::shared_ptr<Scene> SceneRef::Lock() const {
        Assertf(std::this_thread::get_id() == activeSceneManagerThread,
            "SceneRef::Lock() must only be called in SceneManaager thread");
        Assertf(data, "SceneRef points to null SceneData");
        auto result = ptr.lock();
        Assertf(result, "SceneRef points to null Scene: %s (%s)", data->name, data->type);
        return result;
    }

    SceneManager::SceneManager() : RegisteredThread("SceneManager", 30.0) {
        activeSceneManagerThread = std::this_thread::get_id();
        funcs.Register<std::string>("loadscene",
            "Load a scene and replace current scenes",
            [this](std::string scenePath) {
                QueueActionAndBlock(SceneAction::LoadScene, scenePath);
            });
        funcs.Register<std::string>("addscene", "Load a scene", [this](std::string scenePath) {
            QueueActionAndBlock(SceneAction::AddScene, scenePath);
        });
        funcs.Register<std::string>("removescene", "Remove a scene", [this](std::string scenePath) {
            QueueActionAndBlock(SceneAction::RemoveScene, scenePath);
        });
        funcs.Register<std::string>("reloadscene", "Reload current scene", [this](std::string scenePath) {
            QueueActionAndBlock(SceneAction::ReloadScene, scenePath);
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
        bool alreadyShutdown = shutdown.exchange(true);
        if (alreadyShutdown) return;

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
                ZoneStr(item.scenePath);

                if (!item.editSceneCallback) {
                    // Load the System scene from json
                    AddScene(item.scenePath, SceneType::System);
                } else {
                    std::string sceneName(GetSceneName(item.scenePath));

                    auto scene = stagedScenes.Load(sceneName);
                    if (!scene) {
                        {
                            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                            scene = Scene::New(stagingLock,
                                sceneName,
                                item.scenePath,
                                SceneType::System,
                                ScenePriority::System);
                        }
                        stagedScenes.Register(sceneName, scene);
                        scenes[SceneType::System].emplace_back(scene);
                    }

                    {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        item.editSceneCallback(stagingLock, scene);
                    }
                    Tracef("Applying system scene: %s", scene->data->path);
                    scene->ApplyScene();
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::EditStagingScene) {
                ZoneScopedN("EditStagingScene");
                ZoneStr(item.scenePath);
                if (item.editSceneCallback) {
                    std::string sceneName(GetSceneName(item.scenePath));

                    auto scene = stagedScenes.Load(sceneName);
                    if (scene) {
                        if (scene->data->type != SceneType::System) {
                            Tracef("Editing staging scene: %s", scene->data->path);
                            {
                                auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                                item.editSceneCallback(stagingLock, scene);
                            }
                        } else {
                            Errorf("SceneManager::EditStagingScene: Cannot edit system scene: %s", scene->data->path);
                        }
                    } else {
                        Errorf("SceneManager::EditStagingScene: scene %s not found", item.scenePath);
                    }
                } else {
                    Errorf("SceneManager::EditStagingScene called on %s without editSceneCallback", item.scenePath);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::RefreshScenePrefabs) {
                ZoneScopedN("RefreshScenePrefabs");
                ZoneStr(item.scenePath);
                std::string sceneName(GetSceneName(item.scenePath));

                auto scene = stagedScenes.Load(sceneName);
                if (scene) {
                    RefreshPrefabs(scene);
                } else {
                    Errorf("SceneManager::RefreshScenePrefabs: scene %s not found", item.scenePath);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ApplyStagingScene ||
                       item.action == SceneAction::ApplyResetStagingScene) {
                ZoneScopedN("ApplyStagingScene");
                ZoneStr(item.scenePath);
                std::string sceneName(GetSceneName(item.scenePath));

                auto scene = stagedScenes.Load(sceneName);
                if (scene) {
                    if (scene->data->type != SceneType::System) {
                        Tracef("Applying staging scene: %s", scene->data->path);
                        bool resetLive = item.action == SceneAction::ApplyResetStagingScene;
                        if (item.editCallback) {
                            PreloadAndApplyScene(scene,
                                resetLive,
                                [callback = item.editCallback](auto stagingLock, auto liveLock, auto scene) {
                                    callback(liveLock);
                                });
                        } else {
                            PreloadAndApplyScene(scene, resetLive);
                        }
                    } else {
                        Errorf("SceneManager::ApplyStagingScene: Cannot apply system scene: %s", scene->data->path);
                    }
                } else {
                    Errorf("SceneManager::ApplyStagingScene: scene %s not found", item.scenePath);
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::SaveStagingScene) {
                ZoneScopedN("SaveStagingScene");
                ZoneStr(item.scenePath);
                std::string sceneName(GetSceneName(item.scenePath));
                SaveSceneJson(sceneName);
                item.promise.set_value();
            } else if (item.action == SceneAction::SaveLiveScene) {
                ZoneScopedN("SaveLiveScene");
                ZoneStr(item.scenePath);
                SaveLiveSceneJson(item.scenePath);
                item.promise.set_value();
            } else if (item.action == SceneAction::LoadScene) {
                ZoneScopedN("LoadScene");
                ZoneStr(item.scenePath);
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

                if (starts_with(item.scenePath, "saves/")) {
                    ZoneScopedN("ReloadPlayer");
                    if (playerScene) {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                        playerScene->RemoveScene(stagingLock, liveLock);
                        playerScene.reset();
                    }
                    AddScene(item.scenePath, SceneType::Async);

                    playerScene = LoadSceneJson("player", "system/player", SceneType::World);
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
                } else {
                    AddScene(item.scenePath, SceneType::World, [this](auto stagingLock, auto liveLock, auto scene) {
                        // Maybne pause physics here?
                        RespawnPlayer(liveLock);
                    });
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::ReloadScene) {
                ZoneScopedN("ReloadScene");
                ZoneStr(item.scenePath);
                if (item.scenePath.empty()) {
                    // Reload all async and user scenes
                    std::vector<std::pair<std::string, SceneType>> reloadScenes;
                    size_t reloadCount = scenes[SceneType::Async].size() + scenes[SceneType::World].size();
                    reloadScenes.reserve(reloadCount);

                    if (reloadCount > 0) {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                        for (auto &scene : scenes[SceneType::World]) {
                            reloadScenes.emplace_back(scene->data->path, SceneType::World);
                        }
                        for (auto &scene : scenes[SceneType::Async]) {
                            reloadScenes.emplace_back(scene->data->path, SceneType::Async);
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

                        // TODO: Scripts that start transactions in destructor / threads can deadlock when removed from
                        // the scene
                    }

                    for (auto &[path, type] : reloadScenes) {
                        AddScene(path, type);
                    }
                } else {
                    std::string sceneName(GetSceneName(item.scenePath));

                    auto loadedScene = stagedScenes.Load(sceneName);
                    if (loadedScene != nullptr) {
                        auto sceneType = loadedScene->data->type;
                        auto &sceneList = scenes[sceneType];
                        sceneList.erase(std::remove(sceneList.begin(), sceneList.end(), loadedScene));

                        {
                            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                            loadedScene->RemoveScene(stagingLock, liveLock);
                        }

                        loadedScene.reset();
                        Assert(stagedScenes.Drop(sceneName), "Staged scene still in use after removal");

                        AddScene(item.scenePath, sceneType);
                    } else {
                        Errorf("Scene not current loaded: %s", sceneName);
                    }
                }
                item.promise.set_value();
            } else if (item.action == SceneAction::AddScene) {
                ZoneScopedN("AddScene");
                ZoneStr(item.scenePath);
                AddScene(item.scenePath, SceneType::World);
                item.promise.set_value();
            } else if (item.action == SceneAction::RemoveScene) {
                ZoneScopedN("RemoveScene");
                ZoneStr(item.scenePath);
                std::string sceneName(GetSceneName(item.scenePath));

                auto loadedScene = stagedScenes.Load(sceneName);
                if (loadedScene != nullptr) {
                    auto &sceneList = scenes[loadedScene->data->type];
                    sceneList.erase(std::remove(sceneList.begin(), sceneList.end(), loadedScene));

                    {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                        loadedScene->RemoveScene(stagingLock, liveLock);
                    }

                    loadedScene.reset();
                    Assert(stagedScenes.Drop(sceneName), "Staged scene still in use after removal");
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

                playerScene = LoadSceneJson("player", "system/player", SceneType::World);
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
                    bindingsScene->ApplyScene();
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

            for (const ecs::Entity &ent : lock.EntitiesWith<ecs::SceneConnection>()) {
                auto &connection = ent.Get<ecs::SceneConnection>(lock);
                for (auto &[scenePath, signals] : connection.scenes) {
                    for (auto &expr : signals) {
                        if (expr.Evaluate(lock) >= 0.5) {
                            requiredSceneList.emplace(scenePath);
                            break;
                        }
                    }
                }
            }
        }

        scenes[SceneType::Async].clear();
        for (auto &scenePath : requiredSceneList) {
            std::string sceneName(GetSceneName(scenePath));
            auto loadedScene = stagedScenes.Load(sceneName);
            if (loadedScene) {
                if (loadedScene->data->type == SceneType::Async) scenes[SceneType::Async].push_back(loadedScene);
            } else {
                Debugf("Loading scene connection: %s", scenePath);
                AddScene(scenePath, SceneType::Async);
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
            ZoneStr(scene->data->path);
            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
            scene->RemoveScene(stagingLock, liveLock);
            scene.reset();
        });
        ecs::GetEntityRefs().Tick(this->interval);
        ecs::GetSignalManager().Tick(this->interval);
    }

    void SceneManager::QueueAction(SceneAction action, std::string_view sceneName, EditSceneCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(action, sceneName, callback);
    }

    void SceneManager::QueueAction(SceneAction action, std::string_view sceneName, EditCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(action, sceneName, callback);
    }

    void SceneManager::QueueAction(SceneAction action, EditCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(action, callback);
    }

    void SceneManager::QueueAction(VoidCallback callback) {
        std::lock_guard lock(actionMutex);
        if (state != ThreadState::Started) return;
        actionQueue.emplace_back(SceneAction::RunCallback, callback);
    }

    void SceneManager::QueueActionAndBlock(SceneAction action, std::string_view sceneName, EditSceneCallback callback) {
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

    void SceneManager::QueueActionAndBlock(VoidCallback callback) {
        std::future<void> future;
        {
            std::lock_guard lock(actionMutex);
            if (state != ThreadState::Started) return;
            auto &entry = actionQueue.emplace_back(SceneAction::RunCallback, callback);
            future = entry.promise.get_future();
        }
        try {
            future.get();
        } catch (const std::future_error &) {
            Abortf("SceneManager action did not complete");
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
        ZoneStr(scene->data->path);
        {
            std::lock_guard lock(preloadMutex);
            Assertf(!preloadScene,
                "Already preloading %s when trying to preload %s",
                preloadScene->data->path,
                scene->data->path);
            if (state != ThreadState::Started) return;
            preloadScene = scene;
            graphicsPreload.clear();
            physicsPreload.clear();
        }

        if (enableGraphicsPreload) {
            while (!graphicsPreload.test()) {
                graphicsPreload.wait(false);
            }
        }
        if (enablePhysicsPreload) {
            while (!physicsPreload.test()) {
                physicsPreload.wait(false);
            }
        }

        scene->ApplyScene(resetLive, [&](auto &stagingLock, auto &liveLock) {
            if (callback) callback(stagingLock, liveLock, scene);
            {
                std::lock_guard lock(preloadMutex);
                preloadScene.reset();
            }
        });
    }

    std::string_view SceneManager::GetSceneName(std::string_view scenePath) {
        auto delim = scenePath.rfind('/');
        if (delim != std::string::npos) return scenePath.substr(delim + 1);
        return scenePath;
    }

    void SceneManager::RefreshPrefabs(const std::shared_ptr<Scene> &scene) {
        ZoneScopedN("RefreshPrefabs");
        ZoneStr(scene->data->path);
        {
            Tracef("Refreshing scene prefabs: %s", scene->data->path);
            auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();

            // Remove all entities generated by a prefab
            for (auto &e : lock.EntitiesWith<ecs::SceneInfo>()) {
                auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                if (sceneInfo.scene != scene) continue;

                if (sceneInfo.prefabStagingId) scene->RemoveEntity(lock, e);
            }
            auto &scriptManager = ecs::GetScriptManager();
            for (auto &e : lock.EntitiesWith<ecs::Scripts>()) {
                if (!e.Has<ecs::Scripts, ecs::SceneInfo>(lock)) continue;
                auto &sceneInfo = e.Get<ecs::SceneInfo>(lock);
                if (sceneInfo.scene != scene) continue;
                if (sceneInfo.prefabStagingId) continue;

                scriptManager.RunPrefabs(lock, e);
            }
        }
    }

    std::shared_ptr<Scene> SceneManager::LoadSceneJson(const std::string &sceneName,
        const std::string &scenePath,
        SceneType sceneType) {
        Logf("Loading scene: %s", scenePath);

        std::string adjustedPath;
        AssetType adjustedType;
        if (starts_with(scenePath, "saves/")) {
            adjustedPath = scenePath;
            adjustedType = AssetType::External;
        } else {
            adjustedPath = "scenes/" + scenePath;
            adjustedType = AssetType::Bundled;
        }

        auto asset = Assets().Load(adjustedPath + ".json", adjustedType, true)->Get();
        if (!asset) {
            Errorf("Scene not found: %s", scenePath);
            return nullptr;
        }

        picojson::value root;
        string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("Failed to parse scene (%s): %s", scenePath, err);
            return nullptr;
        }
        if (!root.is<picojson::object>()) {
            Errorf("Failed to parse scene (%s): %s", scenePath, root.to_str());
            return nullptr;
        }
        auto &sceneObj = root.get<picojson::object>();

        ScenePriority priority = sceneType == SceneType::System ? ScenePriority::System : ScenePriority::Scene;
        ecs::EntityScope scope(sceneName, "");
        if (sceneObj.count("priority")) {
            json::Load(priority, sceneObj["priority"]);
        }

        ecs::SceneProperties sceneProperties = {};
        if (sceneObj.count("properties")) {
            if (!json::Load(sceneProperties, sceneObj["properties"])) {
                Errorf("Scene contains invalid properties: %s", scenePath);
            }
        }

        std::vector<std::string> libraries;
        if (sceneObj.count("libraries")) {
            if (json::Load(libraries, sceneObj["libraries"])) {
                for (auto &libName : libraries) {
                    if (enableDynamicLibraries) {
                        ecs::GetScriptManager().LoadDynamicLibrary(libName);
                    } else {
                        Warnf("Skipping loading dynamic library: %s", libName);
                    }
                }
            } else {
                Errorf("Scene contains invalid libraries: %s", scenePath);
            }
        }

        std::vector<ecs::FlatEntity> entities;
        if (sceneObj.count("entities")) {
            auto &entityList = sceneObj["entities"];
            for (auto &value : entityList.get<picojson::array>()) {
                auto &entSrc = value.get<picojson::object>();
                auto &entDst = entities.emplace_back();

                if (entSrc.count("name") && entSrc["name"].is<string>()) {
                    ecs::Name name(entSrc["name"].get<string>(), scope);
                    if (name) std::get<std::optional<ecs::Name>>(entDst) = name;
                }

                for (auto &comp : entSrc) {
                    if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                    auto componentType = ecs::LookupComponent(comp.first);
                    if (componentType != nullptr) {
                        if (!componentType->LoadEntity(entDst, comp.second)) {
                            Errorf("LoadScene(%s): Failed to load component, ignoring: %s", scenePath, comp.first);
                        }
                    } else {
                        Errorf("LoadScene(%s): Unknown component, ignoring: %s", scenePath, comp.first);
                    }
                }
            }
        }

        auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();
        auto scene = Scene::New(lock, sceneName, scenePath, sceneType, priority, asset, sceneProperties, libraries);

        std::vector<ecs::Entity> scriptEntities;
        for (auto &flatEnt : entities) {
            auto &name_ptr = std::get<std::optional<ecs::Name>>(flatEnt);
            auto name = name_ptr ? *name_ptr : ecs::Name();

            ecs::Entity entity = scene->NewRootEntity(lock, scene, name);
            if (!entity) {
                // Most llkely a duplicate entity definition
                Errorf("LoadScene(%s): Failed to create entity, ignoring: '%s'", scenePath, name.String());
                continue;
            }

            ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                comp.SetComponent(lock, scope, entity, flatEnt);
            });

            if (entity.Has<ecs::Scripts>(lock)) {
                scriptEntities.push_back(entity);
            }
        }

        auto &scriptManager = ecs::GetScriptManager();
        for (auto &e : scriptEntities) {
            scriptManager.RunPrefabs(lock, e);
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

        picojson::value root;
        string err = picojson::parse(root, bindingConfig->String());
        if (!err.empty()) Abortf("Failed to parse input binding json file: %s", err);
        if (!root.is<picojson::object>()) {
            Abortf("Failed to parse input binding json: %s", root.to_str());
        }

        ecs::EntityScope scope("bindings", "");

        // Only allow signal_output, signal_bindings, and event_bindings to be defined in bindings scene
        static const std::array<const ecs::ComponentBase *, 3> allowedComponents = {
            &ecs::LookupComponent<ecs::SignalOutput>(),
            &ecs::LookupComponent<ecs::SignalBindings>(),
            &ecs::LookupComponent<ecs::EventBindings>(),
        };

        std::vector<ecs::FlatEntity> entities;
        for (auto &[nameStr, value] : root.get<picojson::object>()) {
            ecs::Name name(nameStr, ecs::Name());
            if (!name) {
                Errorf("Binding entity has invalid name, ignoring: %s", nameStr);
                continue;
            } else if (!value.is<picojson::object>()) {
                Errorf("Binding entity has invalid value, ignoring: %s = %s", nameStr, value.to_str());
                continue;
            }

            auto &entSrc = value.get<picojson::object>();
            auto &entDst = entities.emplace_back();
            std::get<std::optional<ecs::Name>>(entDst) = name;

            for (auto *comp : allowedComponents) {
                Assertf(comp,
                    "Missing allowedComponents definition at index: %d",
                    std::find(allowedComponents.begin(), allowedComponents.end(), comp) - allowedComponents.begin());
                if (!entSrc.count(comp->name)) continue;

                if (!comp->LoadEntity(entDst, entSrc[comp->name])) {
                    Errorf("Failed to load binding entity component for '%s', ignoring: %s", nameStr, comp->name);
                }
            }
        }

        auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();
        auto scene = Scene::New(lock,
            "bindings",
            bindingConfig->path.string(),
            SceneType::System,
            ScenePriority::Bindings,
            bindingConfig);

        for (auto &flatEnt : entities) {
            auto &name = std::get<std::optional<ecs::Name>>(flatEnt);
            if (!name || !*name) continue;

            ecs::Entity entity = scene->NewRootEntity(lock, scene, *name);
            if (!entity) {
                // Most llkely a duplicate entity definition
                Errorf("Failed to create binding entity, ignoring: '%s'", name->String());
                continue;
            }

            for (auto *comp : allowedComponents) {
                if (comp) comp->SetComponent(lock, scope, entity, flatEnt);
            }
        }
        return scene;
    }

    std::shared_ptr<Scene> SceneManager::AddScene(std::string scenePath,
        SceneType sceneType,
        OnApplySceneCallback callback) {
        ZoneScoped;
        std::string sceneName(GetSceneName(scenePath));

        ZonePrintf("%s scene: %s", sceneType, sceneName);
        auto loadedScene = stagedScenes.Load(sceneName);
        if (loadedScene != nullptr) {
            Logf("Scene %s already loaded as %s", scenePath, loadedScene->data->path);
            return loadedScene;
        }

        loadedScene = LoadSceneJson(sceneName, scenePath, sceneType);
        if (loadedScene) {
            loadedScene->UpdateSceneProperties();
            PreloadAndApplyScene(loadedScene, false, callback);

            stagedScenes.Register(sceneName, loadedScene);
            scenes[sceneType].emplace_back(loadedScene);
        } else {
            Errorf("Failed to load scene: %s", scenePath);
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

        ecs::Transform spawnTransform = spawn.Get<const ecs::TransformSnapshot>(lock);
        spawnTransform.SetScale(glm::vec3(1));

        auto player = entities::Player.Get(lock);
        if (player.Has<ecs::TransformSnapshot, ecs::TransformTree>(lock)) {
            auto &playerTransform = player.Get<ecs::TransformSnapshot>(lock).globalPose;
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
        {
            auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
            auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();

            if (filterName.empty() || sp::iequals(filterName, "player")) {
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
                        Logf("  -> %s scene", stagingInfo.scene.data->path);
                        stagingId = stagingInfo.nextStagingId;
                    }
                }
            }

            if (filterName.empty() || sp::iequals(filterName, "bindings")) {
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
                        Logf("  -> %s scene", stagingInfo.scene.data->path);
                        stagingId = stagingInfo.nextStagingId;
                    }
                }
            }

            for (size_t sceneTypeI = 0; sceneTypeI < scenes.size(); sceneTypeI++) {
                auto sceneType = static_cast<SceneType>(sceneTypeI);
                std::string typeName(magic_enum::enum_name<SceneType>(sceneType));

                if (filterName.empty() || sp::iequals(filterName, typeName)) {
                    for (auto scene : scenes[sceneType]) {
                        Logf("Entities from %s scene: %s", typeName, scene->data->path);
                        for (auto &e : liveLock.EntitiesWith<ecs::Name>()) {
                            if (!e.Has<ecs::Name, ecs::SceneInfo>(liveLock)) continue;
                            auto &sceneInfo = e.Get<ecs::SceneInfo>(liveLock);
                            if (sceneInfo.scene != scene) continue;

                            Logf("  %s", ecs::ToString(liveLock, e));
                            auto stagingId = sceneInfo.nextStagingId;
                            while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                                auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                                Assert(stagingInfo.scene, "Missing SceneInfo scene on entity");
                                Logf(" -> %s scene (%s type)",
                                    stagingInfo.scene.data->path,
                                    stagingInfo.scene.data->type);
                                stagingId = stagingInfo.nextStagingId;
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace sp
