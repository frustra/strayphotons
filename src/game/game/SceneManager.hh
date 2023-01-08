#pragma once

#include "console/CFunc.hh"
#include "core/DispatchQueue.hh"
#include "core/EnumTypes.hh"
#include "core/LockFreeMutex.hh"
#include "core/Logging.hh"
#include "core/PreservingMap.hh"
#include "core/RegisteredThread.hh"
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
        ApplyStagingScene, // Arguments: (sceneName)
        SaveStagingScene, // Arguments: (sceneName)
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
        SceneManager(bool skipPreload = false);
        ~SceneManager();
        void Shutdown();

        std::vector<SceneRef> GetActiveScenes();

        using EditSceneCallback = std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)>;
        using EditCallback = std::function<void(ecs::Lock<ecs::AddRemove>)>;
        using VoidCallback = std::function<void()>;
        void QueueAction(SceneAction action, std::string sceneName = "", EditSceneCallback callback = nullptr);
        void QueueAction(SceneAction action, EditCallback callback);
        void QueueAction(SceneAction action, VoidCallback callback);
        void QueueActionAndBlock(SceneAction action, std::string sceneName = "", EditSceneCallback callback = nullptr);
        void QueueActionAndBlock(SceneAction action, VoidCallback callback);

        using ScenePreloadCallback = std::function<bool(ecs::Lock<ecs::ReadAll>, std::shared_ptr<Scene>)>;
        void PreloadSceneGraphics(ScenePreloadCallback callback);
        void PreloadScenePhysics(ScenePreloadCallback callback);

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

        std::shared_ptr<Scene> LoadSceneJson(const std::string &name, SceneType sceneType);
        void SaveSceneJson(const std::string &name);

        std::shared_ptr<Scene> LoadBindingsJson();

        std::shared_ptr<Scene> AddScene(std::string name, SceneType sceneType, OnApplySceneCallback callback = nullptr);

    private:
        struct QueuedAction {
            SceneAction action;

            std::string sceneName;
            EditSceneCallback editSceneCallback;
            EditCallback editCallback;
            VoidCallback voidCallback;
            std::promise<void> promise;

            QueuedAction(SceneAction action, std::string sceneName, EditSceneCallback editSceneCallback = nullptr)
                : action(action), sceneName(sceneName), editSceneCallback(editSceneCallback) {}
            QueuedAction(SceneAction action, EditCallback editCallback) : action(action), editCallback(editCallback) {}
            QueuedAction(SceneAction action, VoidCallback voidCallback) : action(action), voidCallback(voidCallback) {}
        };

        LockFreeMutex actionMutex, preloadMutex;
        std::deque<QueuedAction> actionQueue;
        std::shared_ptr<Scene> preloadScene;
        std::atomic_flag physicsPreload, graphicsPreload;
        bool skipPreload;

        LockFreeMutex activeSceneMutex;
        std::vector<SceneRef> activeSceneCache;

        PreservingMap<std::string, Scene, 1000> stagedScenes;
        using SceneList = std::vector<std::shared_ptr<Scene>>;
        EnumArray<SceneList, SceneType> scenes;
        std::shared_ptr<Scene> playerScene, bindingsScene;
        CFuncCollection funcs;

        friend class SceneInfo;
        friend class Scene;
    };

    SceneManager &GetSceneManager();
} // namespace sp
