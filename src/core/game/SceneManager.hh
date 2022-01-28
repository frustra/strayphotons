#pragma once

#include "console/CFunc.hh"
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
    class Script;

    static const char *const InputBindingConfigPath = "input_bindings.json";

    enum class SceneAction {
        AddSystemScene, // Arguments: (sceneName, applyCallback)
        LoadScene, // Arguments: (sceneName)
        ReloadScene, // Arguments: (sceneName)
        AddScene, // Arguments: (sceneName)
        RemoveScene, // Arguments: (sceneName)
        LoadPlayer, // Arguments: ()
        LoadBindings, // Arguments: ()
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
                case SceneAction::LoadPlayer:
                    return "LoadPlayer";
                case SceneAction::LoadBindings:
                    return "LoadBindings";
                default:
                    return "SceneAction::INVALID";
                }
            }
        };
    } // namespace logging

    class SceneManager : public RegisteredThread {
    public:
        SceneManager(ecs::ECS &liveWorld, ecs::ECS &stagingWorld);
        ~SceneManager();

        using ApplySceneCallback = std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)>;
        void QueueAction(SceneAction action, std::string sceneName = "", ApplySceneCallback callback = nullptr);
        void QueueActionAndBlock(SceneAction action, std::string sceneName = "", ApplySceneCallback callback = nullptr);

        void RespawnPlayer(Tecs::Entity player);

        void PrintScene(std::string sceneName);

    private:
        void RunSceneActions();
        void Frame() override;

        struct QueuedAction {
            SceneAction action;

            std::string sceneName;
            ApplySceneCallback applyCallback;
        };

        void LoadSceneJson(const std::string &name,
            SceneType sceneType,
            ecs::SceneInfo::Priority priority,
            std::function<void(std::shared_ptr<Scene>)> callback);

        void LoadBindingsJson(std::function<void(std::shared_ptr<Scene>)> callback);

        std::shared_ptr<Scene> AddScene(std::string name, SceneType sceneType);

        void TranslateSceneByConnection(const std::shared_ptr<Scene> &scene);

    private:
        ecs::ECS &liveWorld;
        ecs::ECS &stagingWorld;
        Tecs::Entity player;

        LockFreeMutex queueMutex;
        std::deque<std::pair<QueuedAction, std::promise<void>>> actionQueue;

        PreservingMap<std::string, Scene, 1000> stagedScenes;
        using SceneList = std::vector<std::shared_ptr<Scene>>;
        EnumArray<SceneList, SceneType> scenes;
        std::shared_ptr<Scene> playerScene, bindingsScene;
        CFuncCollection funcs;

        friend class Scene;
    };

    SceneManager &GetSceneManager();
} // namespace sp
