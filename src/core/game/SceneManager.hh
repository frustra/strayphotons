#pragma once

#include "console/CFunc.hh"
#include "console/ConsoleBindingManager.hh"
#include "ecs/Ecs.hh"

#include <future>
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

        std::future<std::shared_ptr<Scene>> LoadSceneJson(const std::string &name);
        std::shared_ptr<Scene> LoadBindingJson(std::string bindingConfigPath);

        void LoadScene(std::string name);
        void ReloadScene(std::string name);
        void AddScene(std::string name);
        void RemoveScene(std::string name);

        void LoadPlayer();
        void RespawnPlayer();

        void PrintScenes();

        Tecs::Entity GetPlayer() const {
            return player;
        }

        const std::shared_ptr<Scene> &GetPlayerScene() const {
            return playerScene;
        }

    private:
        ecs::ECS stagingWorld;

        ConsoleBindingManager consoleBinding;

        robin_hood::unordered_flat_map<std::string, std::shared_ptr<Scene>> scenes;
        std::shared_ptr<Scene> playerScene, systemScene;
        Tecs::Entity player;
        CFuncCollection funcs;
    };
} // namespace sp
