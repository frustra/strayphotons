#include "GameLogic.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"

namespace sp {
    GameLogic::GameLogic(bool stepMode) : RegisteredThread("GameLogic", 120.0, true), stepMode(stepMode) {
        if (stepMode) {
            funcs.Register<unsigned int>("steplogic",
                "Advance the game logic by N frames, default is 1",
                [this](unsigned int arg) {
                    this->Step(std::max(1u, arg));
                });
        }
    }

    void GameLogic::StartThread() {
        RegisteredThread::StartThread(stepMode);
    }

    void GameLogic::Frame() {
        ZoneScoped;
        {
            auto lock = ecs::StartTransaction<ecs::WriteAll>();
            ecs::GetScriptManager().RunOnTick(lock, interval);
        }
    }
} // namespace sp
