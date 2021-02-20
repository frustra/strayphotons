#pragma once

#include "Common.hh"
#include "core/CFunc.hh"
#include "ecs/systems/HumanControlSystem.hh"

#include <ecs/Ecs.hh>

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
        void UnloadScene();
        void PrintDebug();

        void SetSignal(string args);
        void ClearSignal(string args);

        Tecs::Entity GetPlayer() {
            return player;
        }

    private:
        Game *game;
        InputManager *input = nullptr;
        ecs::HumanControlSystem humanControlSystem;
        shared_ptr<Scene> scene;
        Tecs::Entity player;
        Tecs::Entity flashlight;
        CFuncCollection funcs;
    };
} // namespace sp
