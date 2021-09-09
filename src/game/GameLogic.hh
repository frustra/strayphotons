#pragma once

#include "core/CFunc.hh"
#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Game;
    class Scene;
    class Script;

    class GameLogic {
    public:
        GameLogic(Game *game);
        ~GameLogic();

        void Init(Script *startupScript = nullptr);
#ifdef SP_INPUT_SUPPORT
        void HandleInput();
#endif
        bool Frame(double dtSinceLastFrame);

        void LoadPlayer();
        void LoadScene(std::string name);
        void ReloadScene(std::string arg);
        void PrintDebug();

        void PrintFocus();
        void AcquireFocus(std::string args);
        void ReleaseFocus(std::string args);

        void SetSignal(std::string args);
        void ClearSignal(std::string args);

        void TraceTecs(std::string args);

        Tecs::Entity GetPlayer() {
            return player;
        }

    private:
        Game *game;
        std::shared_ptr<Scene> scene, playerScene;
        Tecs::Entity player;
        Tecs::Entity flashlight;
        CFuncCollection funcs;
    };
} // namespace sp
