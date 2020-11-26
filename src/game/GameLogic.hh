#pragma once

#include "Common.hh"
#include "core/CFunc.hh"
#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/systems/LightGunSystem.hh"

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

        void LoadScene(string name);
        void ReloadScene(string arg);
        void PrintDebug();

        void OpenBarrier(string name);
        void CloseBarrier(string name);

        void OpenDoor(string name);
        void CloseDoor(string name);
        void SetSignal(string name);

        ecs::Entity GetPlayer();

    private:
        ecs::Entity CreateGameLogicEntity();

    private:
        Game *game;
        InputManager *input = nullptr;
        ecs::HumanControlSystem humanControlSystem;
        ecs::LightGunSystem lightGunSystem;
        shared_ptr<Scene> scene;
        ecs::Entity flashlight;
        CFuncCollection funcs;
    };
} // namespace sp
