#pragma once

#include "core/CFunc.hh"
#include "ecs/Ecs.hh"
#include "game/systems/HumanControlSystem.hh"

#include <memory>

namespace sp {
    class Game;
    class Scene;
    class Script;
    class InputManager;

    class GameLogic {
    public:
        GameLogic(Game *game);
        ~GameLogic();

        void Init(Script *startupScript = nullptr);
        void HandleInput();
        bool Frame(double dtSinceLastFrame);

        void ResetPlayer();
        void LoadScene(string name);
        void ReloadScene(string arg);
        void PrintDebug();

        void SetSignal(string args);
        void ClearSignal(string args);

        Tecs::Entity GetPlayer() {
            return player;
        }

    private:
        Game *game;
        InputManager *input = nullptr;
        std::shared_ptr<Scene> scene;
        Tecs::Entity player;
        Tecs::Entity flashlight;
        CFuncCollection funcs;
    };
} // namespace sp
