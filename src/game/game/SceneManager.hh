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
        ApplySystemScene, // Arguments: (sceneName, applyCallback)
        EditStagingScene, // Arguments: (sceneName, applyCallback)
        SaveStagingScene, // Arguments: (sceneName)
        EditLiveECS, // Arguments: (editCallback)
        ApplyScene, // Arguments: (sceneName)
        LoadScene, // Arguments: (sceneName)
        ReloadScene, // Arguments: (sceneName)
        AddScene, // Arguments: (sceneName)
        RemoveScene, // Arguments: (sceneName)
        ReloadPlayer, // Arguments: ()
        ReloadBindings, // Arguments: ()
        SyncScene, // Arguments: ()
    };

    class SceneManager : public RegisteredThread {
    public:
        SceneManager(bool skipPreload = false);
        ~SceneManager();
        void Shutdown();

        using PreApplySceneCallback = std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)>;
        using EditSceneCallback = std::function<void(ecs::Lock<ecs::WriteAll>)>;
        void QueueAction(SceneAction action, std::string sceneName = "", PreApplySceneCallback callback = nullptr);
        void QueueAction(SceneAction action, EditSceneCallback callback);
        void QueueActionAndBlock(SceneAction action,
            std::string sceneName = "",
            PreApplySceneCallback callback = nullptr);

        using ScenePreloadCallback = std::function<bool(ecs::Lock<ecs::ReadAll>, std::shared_ptr<Scene>)>;
        void PreloadSceneGraphics(ScenePreloadCallback callback);
        void PreloadScenePhysics(ScenePreloadCallback callback);

    private:
        void RunSceneActions();
        void UpdateSceneConnections();

        using OnApplySceneCallback = std::function<void(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>,
            ecs::Lock<ecs::AddRemove>,
            std::shared_ptr<Scene>)>;
        void PreloadAndApplyScene(const std::shared_ptr<Scene> &scene,
            bool resetLive = false,
            OnApplySceneCallback callback = nullptr);

        void Frame() override;

        void PrintScene(std::string sceneName);
        void RespawnPlayer(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>> lock,
            ecs::Entity player);

        std::shared_ptr<Scene> LoadSceneJson(const std::string &name, SceneType sceneType);
        void SaveSceneJson(const std::string &name);

        std::shared_ptr<Scene> LoadBindingsJson();

        std::shared_ptr<Scene> AddScene(std::string name, SceneType sceneType, OnApplySceneCallback callback = nullptr);

    private:
        struct QueuedAction {
            SceneAction action;

            std::string sceneName;
            PreApplySceneCallback applyCallback;
            EditSceneCallback editCallback;
            std::promise<void> promise;

            QueuedAction(SceneAction action, std::string sceneName, PreApplySceneCallback applyCallback = nullptr)
                : action(action), sceneName(sceneName), applyCallback(applyCallback) {}
            QueuedAction(SceneAction action, EditSceneCallback editCallback)
                : action(action), editCallback(editCallback) {}
        };

        ecs::Entity player;

        LockFreeMutex actionMutex, preloadMutex;
        std::deque<QueuedAction> actionQueue;
        std::shared_ptr<Scene> preloadScene;
        std::atomic_flag physicsPreload, graphicsPreload;
        bool skipPreload;

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
