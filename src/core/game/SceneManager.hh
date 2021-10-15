#pragma once

#include "core/CFunc.hh"
#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Game;
    class Scene;
    class Script;

    class SceneManager {
    public:
        SceneManager();

#ifdef SP_INPUT_SUPPORT
        void HandleInput();
#endif
        bool Frame(double dtSinceLastFrame);

        void LoadPlayer();
        void LoadScene(std::string name);
        void ReloadScene();
        void ReloadPlayer();

        void PrintScene();

        Tecs::Entity GetPlayer() {
            return player;
        }

    private:
        std::shared_ptr<Scene> scene, playerScene;
        Tecs::Entity player;
        CFuncCollection funcs;
    };
} // namespace sp
