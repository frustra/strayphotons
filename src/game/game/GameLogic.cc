#include "GameLogic.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"

namespace sp {
    GameLogic::GameLogic(bool stepMode) : RegisteredThread("GameLogic", 120.0), stepMode(stepMode) {
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
            auto lock = ecs::StartTransaction<ecs::Write<ecs::Script, ecs::EventInput>>();

            for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
                if (!entity.Has<ecs::Script, ecs::EventInput>(lock)) continue;
                auto &readScript = entity.Get<const ecs::Script>(lock);
                for (size_t i = 0; i < readScript.scripts.size(); i++) {
                    auto &readState = readScript.scripts[i];
                    if (!readState.events.empty() && !readState.eventQueue) {
                        auto &eventInput = entity.Get<ecs::EventInput>(lock);
                        auto &writeScript = entity.Get<ecs::Script>(lock);
                        auto &writeState = writeScript.scripts[i];
                        writeState.eventQueue = ecs::NewEventQueue();
                        for (auto &event : writeState.events) {
                            eventInput.Register(lock, writeState.eventQueue, event);
                        }
                    }
                }
            }
        }
        {
            // TODO: Change this to exclude Write<EventInput>
            auto lock = ecs::StartTransaction<ecs::WriteAll>();
            for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
                auto &script = entity.Get<ecs::Script>(lock);
                script.OnTick(lock, entity, interval);
            }
        }
    }
} // namespace sp
