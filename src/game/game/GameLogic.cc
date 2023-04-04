#include "GameLogic.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"

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
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Scripts>, ecs::Write<ecs::EventInput>>();

            for (auto &entity : lock.EntitiesWith<ecs::Scripts>()) {
                if (!entity.Has<ecs::Scripts, ecs::EventInput>(lock)) continue;
                auto &scripts = entity.Get<ecs::Scripts>(lock);
                for (auto &instance : scripts.scripts) {
                    instance.RegisterEvents(lock, entity);
                }
            }
        }
        {
            // TODO: Change this to exclude Write<EventInput>
            auto lock = ecs::StartTransaction<ecs::WriteAll>();
            for (auto &entity : lock.EntitiesWith<ecs::Scripts>()) {
                auto &scripts = entity.Get<ecs::Scripts>(lock);
                scripts.OnTick(ecs::EntityLock<ecs::WriteAll>(lock, entity), interval);
            }
        }
    }
} // namespace sp
