#pragma once

#include "core/CFunc.hh"
#include "core/input/ConsoleBindingManager.hh"
#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Game;
    class Scene;
    class Script;

    static const char *const InputBindingConfigPath = "input_bindings.json";

    class SceneManager {
    public:
        SceneManager();

        std::shared_ptr<Scene> LoadSceneJson(const std::string &name, ecs::Lock<ecs::AddRemove> lock, ecs::Owner owner);
        std::shared_ptr<Scene> LoadBindingJson(std::string bindingConfigPath);

        void LoadPlayer();
        void LoadScene(std::string name);
        void ReloadScene();
        void ReloadPlayer();

        void PrintScene();

        Tecs::Entity GetPlayer() {
            return player;
        }

    private:
        ConsoleBindingManager consoleBinding;

        std::shared_ptr<Scene> scene, playerScene;
        Tecs::Entity player;
        CFuncCollection funcs;
    };
} // namespace sp
