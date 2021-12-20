#pragma once

#include "console/CFunc.hh"
#include "core/EnumArray.hh"
#include "core/PreservingMap.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/SceneType.hh"

#include <functional>
#include <memory>
#include <robin_hood.h>
#include <shared_mutex>

namespace sp {
    class Game;
    class Scene;
    class Script;

    static const char *const InputBindingConfigPath = "input_bindings.json";

    class SceneManager : public RegisteredThread {
    public:
        SceneManager(ecs::ECS &liveWorld, ecs::ECS &stagingWorld);
        ~SceneManager();

        void AddSystemScene(std::string sceneName,
            std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)> callback);

        void LoadScene(std::string name);
        void ReloadScene(std::string name);
        void AddScene(std::string name,
            SceneType sceneType,
            std::function<void(std::shared_ptr<Scene>)> callback = nullptr);
        void RemoveScene(std::string name);

        Tecs::Entity LoadPlayer();
        void RespawnPlayer();

        void LoadBindings();

        void PrintScene(std::string sceneName);

    private:
        void Frame() override;

        void LoadSceneJson(const std::string &name,
            SceneType sceneType,
            ecs::SceneInfo::Priority priority,
            std::function<void(std::shared_ptr<Scene>)> callback);
        void LoadBindingsJson(std::function<void(std::shared_ptr<Scene>)> callback);

        void TranslateSceneByConnection(const std::shared_ptr<Scene> &scene);

    private:
        ecs::ECS &liveWorld;
        ecs::ECS &stagingWorld;
        Tecs::Entity player;

        std::shared_mutex liveMutex;

        PreservingMap<std::string, Scene, 1000> stagedScenes;
        using SceneList = std::vector<std::shared_ptr<Scene>>;
        EnumArray<SceneList, SceneType> scenes;
        std::shared_ptr<Scene> playerScene, bindingsScene;
        CFuncCollection funcs;

        friend class Scene;
    };

    SceneManager &GetSceneManager();
} // namespace sp
