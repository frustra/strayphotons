#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"

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
        SceneManager();

        void LoadSceneJson(const std::string &name, std::function<void(std::shared_ptr<Scene>)> callback);
        std::shared_ptr<Scene> LoadBindingJson(std::string bindingConfigPath);

        void AddToSystemScene(std::function<void(ecs::Lock<ecs::AddRemove>, std::shared_ptr<Scene>)> callback);

        void LoadScene(std::string name);
        void ReloadScene(std::string name);
        void AddScene(std::string name, std::function<void(std::shared_ptr<Scene>)> callback = nullptr);
        void RemoveScene(std::string name);

        void LoadPlayer();
        void RespawnPlayer();

        void PrintScene(std::string sceneName);

        const Tecs::Entity &GetPlayer() const {
            return player;
        }

    private:
        ecs::ECS stagingWorld;

        robin_hood::unordered_flat_map<std::string, std::shared_ptr<Scene>> scenes;
        std::shared_ptr<Scene> playerScene, systemScene;
        Tecs::Entity player;
        CFuncCollection funcs;
    };

    SceneManager &GetSceneManager();
} // namespace sp
