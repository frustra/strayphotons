#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/SceneInfo.hh"

#include <functional>
#include <memory>
#include <robin_hood.h>

namespace sp {
    class Game;
    class Scene;
    class Script;

    static const char *const InputBindingConfigPath = "input_bindings.json";

    class SceneManager {
    public:
        SceneManager(ecs::ECS &liveWorld, ecs::ECS &stagingWorld);

        void LoadSceneJson(const std::string &name,
            ecs::SceneInfo::Priority priority,
            std::function<void(std::shared_ptr<Scene>)> callback);
        void LoadBindingsJson(std::function<void(std::shared_ptr<Scene>)> callback);

        void AddSystemScene(std::string sceneName,
            std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)> callback);
        void RemoveSystemScene(std::string sceneName);

        void LoadScene(std::string name);
        void ReloadScene(std::string name);
        void AddScene(std::string name, std::function<void(std::shared_ptr<Scene>)> callback = nullptr);
        void RemoveScene(std::string name);

        void LoadPlayer();
        void RespawnPlayer();

        void LoadBindings();

        void PrintScene(std::string sceneName);

        const Tecs::Entity &GetPlayer() const {
            return player;
        }

    private:
        ecs::ECS &liveWorld;
        ecs::ECS &stagingWorld;

        robin_hood::unordered_flat_map<std::string, std::shared_ptr<Scene>> scenes;
        robin_hood::unordered_flat_map<std::string, std::shared_ptr<Scene>> systemScenes;
        std::shared_ptr<Scene> playerScene, bindingsScene;
        Tecs::Entity player;
        CFuncCollection funcs;
    };

    SceneManager &GetSceneManager();
} // namespace sp
