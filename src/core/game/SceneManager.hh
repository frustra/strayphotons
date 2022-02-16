#pragma once

#include "console/CFunc.hh"
#include "core/DispatchQueue.hh"
#include "core/EnumArray.hh"
#include "core/LockFreeMutex.hh"
#include "core/Logging.hh"
#include "core/PreservingMap.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/SceneType.hh"

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <robin_hood.h>
#include <shared_mutex>

namespace sp {
    class Game;
    class Scene;

    static const char *const InputBindingConfigPath = "input_bindings.json";

    enum class SceneAction {
        AddSystemScene, // Arguments: (sceneName, applyCallback)
        LoadScene, // Arguments: (sceneName)
        ReloadScene, // Arguments: (sceneName)
        AddScene, // Arguments: (sceneName)
        RemoveScene, // Arguments: (sceneName)
        ReloadPlayer, // Arguments: ()
        ReloadBindings, // Arguments: ()
        SyncScene, // Arguments: ()
        Count,
    };

    namespace logging {
        template<>
        struct stringify<SceneAction> : std::true_type {
            static const char *to_string(const SceneAction &t) {
                switch (t) {
                case SceneAction::AddSystemScene:
                    return "AddSystemScene";
                case SceneAction::LoadScene:
                    return "LoadScene";
                case SceneAction::ReloadScene:
                    return "ReloadScene";
                case SceneAction::AddScene:
                    return "AddScene";
                case SceneAction::RemoveScene:
                    return "RemoveScene";
                case SceneAction::ReloadPlayer:
                    return "ReloadPlayer";
                case SceneAction::ReloadBindings:
                    return "ReloadBindings";
                case SceneAction::SyncScene:
                    return "SyncScene";
                default:
                    return "SceneAction::INVALID";
                }
            }
        };
    } // namespace logging

    class SceneManager : public RegisteredThread {
    public:
        SceneManager(ecs::ECS &liveWorld, ecs::ECS &stagingWorld, bool skipPreload = false);
        ~SceneManager();

        using PreApplySceneCallback = std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)>;
        void QueueAction(SceneAction action, std::string sceneName = "", PreApplySceneCallback callback = nullptr);
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
        void PreloadAndApplyScene(const std::shared_ptr<Scene> &scene, OnApplySceneCallback callback = nullptr);

        void Frame() override;

        void PrintScene(std::string sceneName);
        void RespawnPlayer(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::TransformSnapshot, ecs::TransformTree>> lock,
            Tecs::Entity player);

        std::shared_ptr<Scene> LoadSceneJson(const std::string &name,
            SceneType sceneType,
            ecs::SceneInfo::Priority priority);

        std::shared_ptr<Scene> LoadBindingsJson();

        std::shared_ptr<Scene> AddScene(std::string name, SceneType sceneType, OnApplySceneCallback callback = nullptr);

        void TranslateSceneByConnection(const std::shared_ptr<Scene> &scene);

    private:
        struct QueuedAction {
            SceneAction action;

            std::string sceneName;
            PreApplySceneCallback callback;
            std::promise<void> promise;
        };

        ecs::ECS &liveWorld;
        ecs::ECS &stagingWorld;
        Tecs::Entity player;

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

        friend class Scene;
    };

    SceneManager &GetSceneManager();
} // namespace sp
