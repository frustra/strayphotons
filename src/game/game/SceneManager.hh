/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/DispatchQueue.hh"
#include "common/EnumTypes.hh"
#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"
#include "common/PreservingMap.hh"
#include "common/RegisteredThread.hh"
#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/Scene.hh"
#include "game/SceneRef.hh"

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <robin_hood.h>
#include <shared_mutex>

namespace sp {
    class Game;

    static const char *const InputBindingConfigPath = "input_bindings.json";

    enum class SceneAction {
        ApplySystemScene, // Arguments: (sceneName, EditSceneCallback)
        EditStagingScene, // Arguments: (sceneName, EditSceneCallback)
        RefreshScenePrefabs, // Arguments: (sceneName)
        ApplyResetStagingScene, // Arguments: (sceneName, [EditCallback])
        ApplyStagingScene, // Arguments: (sceneName, [EditCallback])
        SaveStagingScene, // Arguments: (sceneName)
        SaveLiveScene, // Arguments: (outputPath)
        LoadScene, // Arguments: (sceneName)
        ReloadScene, // Arguments: (sceneName)
        AddScene, // Arguments: (sceneName)
        RemoveScene, // Arguments: (sceneName)
        RespawnPlayer, // Arguemnts: ()
        ReloadPlayer, // Arguments: ()
        ReloadBindings, // Arguments: ()
        SyncScene, // Arguments: ()
        RunCallback, // Arguments: (VoidCallback)
    };

    class SceneManager : public RegisteredThread {
    public:
        SceneManager();
        ~SceneManager();
        void Shutdown();

        std::vector<SceneRef> GetActiveScenes();

        using EditSceneCallback = std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)>;
        using EditCallback = std::function<void(ecs::Lock<ecs::AddRemove>)>;
        using VoidCallback = std::function<void()>;
        void QueueAction(SceneAction action, std::string sceneName = "", EditSceneCallback callback = nullptr);
        void QueueAction(SceneAction action, std::string sceneName, EditCallback callback);
        void QueueAction(SceneAction action, EditCallback callback);
        void QueueAction(VoidCallback callback);
        void QueueActionAndBlock(SceneAction action, std::string sceneName = "", EditSceneCallback callback = nullptr);
        void QueueActionAndBlock(VoidCallback callback);

        using ScenePreloadCallback = std::function<bool(ecs::Lock<ecs::ReadAll>, std::shared_ptr<Scene>)>;
        void PreloadSceneGraphics(ScenePreloadCallback callback);
        void PreloadScenePhysics(ScenePreloadCallback callback);

        void DisableGraphicsPreload() {
            enableGraphicsPreload = false;
        }

        void DisablePhysicsPreload() {
            enablePhysicsPreload = false;
        }

        static std::string_view GetSceneName(std::string_view scenePath);

    private:
        void RunSceneActions();
        void UpdateSceneConnections();
        void RunPrefabs(ecs::Lock<ecs::AddRemove> lock, ecs::Entity ent);

        using OnApplySceneCallback = std::function<void(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>,
            ecs::Lock<ecs::AddRemove>,
            std::shared_ptr<Scene>)>;
        void PreloadAndApplyScene(const std::shared_ptr<Scene> &scene,
            bool resetLive = false,
            OnApplySceneCallback callback = nullptr);
        void RefreshPrefabs(const std::shared_ptr<Scene> &scene);

        bool ThreadInit() override;
        void Frame() override;

        void PrintScene(std::string sceneName);
        void RespawnPlayer(
            ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>> lock);

        std::shared_ptr<Scene> LoadSceneJson(const std::string &name, const std::string &path, SceneType sceneType);
        void SaveSceneJson(const std::string &name);
        void SaveLiveSceneJson(const std::string &path);

        std::shared_ptr<Scene> LoadBindingsJson();

        std::shared_ptr<Scene> AddScene(std::string name, SceneType sceneType, OnApplySceneCallback callback = nullptr);

    private:
        struct QueuedAction {
            SceneAction action;

            std::string scenePath;
            EditSceneCallback editSceneCallback;
            EditCallback editCallback;
            VoidCallback voidCallback;
            std::promise<void> promise;

            QueuedAction(SceneAction action, std::string scenePath, EditSceneCallback editSceneCallback = nullptr)
                : action(action), scenePath(scenePath), editSceneCallback(editSceneCallback) {}
            QueuedAction(SceneAction action, std::string scenePath, EditCallback editCallback)
                : action(action), scenePath(scenePath), editCallback(editCallback) {}
            QueuedAction(SceneAction action, EditCallback editCallback) : action(action), editCallback(editCallback) {}
            QueuedAction(SceneAction action, VoidCallback voidCallback) : action(action), voidCallback(voidCallback) {}
        };

        std::atomic_bool shutdown;
        LockFreeMutex actionMutex, preloadMutex;
        std::deque<QueuedAction> actionQueue;
        std::shared_ptr<Scene> preloadScene;
        std::atomic_flag graphicsPreload, physicsPreload;
        bool enableGraphicsPreload = true;
        bool enablePhysicsPreload = true;

        LockFreeMutex activeSceneMutex;
        std::vector<SceneRef> activeSceneCache;

        PreservingMap<std::string, Scene, 1000> stagedScenes;
        using SceneList = std::vector<std::shared_ptr<Scene>>;
        EnumArray<SceneList, SceneType> scenes;
        std::shared_ptr<Scene> playerScene, bindingsScene;
        CFuncCollection funcs;

        friend class Scene;
    };

    SceneManager &GetSceneManager();
} // namespace sp
