#include "GameLogic.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"

namespace sp {
    GameLogic::GameLogic(bool stepMode) : RegisteredThread("GameLogic", 120.0), stepMode(stepMode) {
        funcs.Register(this, "printdebug", "Print some debug info about the player", &GameLogic::PrintDebug);
        funcs.Register(this, "printevents", "Print out the current state of event queues", &GameLogic::PrintEvents);
        funcs.Register(this, "printsignals", "Print out the values and bindings of signals", &GameLogic::PrintSignals);

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
        auto lock = ecs::World.StartTransaction<ecs::WriteAll>();
        for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
            auto &script = entity.Get<ecs::Script>(lock);
            script.OnTick(lock, entity, interval);
        }
    }

    void GameLogic::PrintDebug() {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::TransformSnapshot, ecs::CharacterController, ecs::PhysicsQuery>>();
        auto player = ecs::EntityWith<ecs::Name>(lock, "player.player");
        auto flatview = ecs::EntityWith<ecs::Name>(lock, "player.flatview");
        if (flatview.Has<ecs::TransformSnapshot>(lock)) {
            auto &transform = flatview.Get<ecs::TransformSnapshot>(lock);
            auto position = transform.GetPosition();
            Logf("Flatview position: [%f, %f, %f]", position.x, position.y, position.z);
        }
        if (player.Has<ecs::TransformSnapshot>(lock)) {
            auto &transform = player.Get<ecs::TransformSnapshot>(lock);
            auto position = transform.GetPosition();
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            if (player.Has<ecs::CharacterController>(lock)) {
                auto &controller = player.Get<ecs::CharacterController>(lock);
                if (controller.pxController) {
                    auto pxFeet = controller.pxController->getFootPosition();
                    Logf("Player physics position: [%f, %f, %f]", pxFeet.x, pxFeet.y, pxFeet.z);
                    auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
                    Logf("Player velocity: [%f, %f, %f]",
                        userData->actorData.velocity.x,
                        userData->actorData.velocity.y,
                        userData->actorData.velocity.z);
                    Logf("Player on ground: %s", userData->onGround ? "true" : "false");
                } else {
                    Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
                }
            } else {
                Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
            }
#else
            Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
#endif
        } else {
            Logf("Scene has no valid player");
        }

        if (flatview.Has<ecs::PhysicsQuery>(lock)) {
            auto &query = flatview.Get<ecs::PhysicsQuery>(lock);
            if (query.raycastHitTarget) Logf("Looking at: %s", ecs::ToString(lock, query.raycastHitTarget));
        }
    }

    void GameLogic::PrintEvents(std::string entityName) {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::EventInput, ecs::EventBindings, ecs::FocusLayer, ecs::FocusLock>>();

        auto &focusLock = lock.Get<ecs::FocusLock>();
        for (auto ent : lock.EntitiesWith<ecs::EventInput>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            if (ent.Has<ecs::FocusLayer>(lock)) {
                auto &layer = ent.Get<ecs::FocusLayer>(lock);
                std::stringstream ss;
                ss << layer;
                if (focusLock.HasPrimaryFocus(layer)) {
                    Logf("Event input %s: (has primary focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else if (focusLock.HasFocus(layer)) {
                    Logf("Event input %s: (has focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else {
                    Logf("Event input %s: (no focus: %s)", ecs::ToString(lock, ent), ss.str());
                }
            } else {
                Logf("Event input %s: (no focus layer)", ecs::ToString(lock, ent));
            }

            auto &input = ent.Get<ecs::EventInput>(lock);
            for (auto &[eventName, queue] : input.events) {
                if (queue.empty()) {
                    Logf("  %s: empty", eventName);
                } else {
                    Logf("  %s: %u events", eventName, queue.size());
                }
            }
        }

        for (auto ent : lock.EntitiesWith<ecs::EventBindings>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            if (ent.Has<ecs::FocusLayer>(lock)) {
                auto &layer = ent.Get<ecs::FocusLayer>(lock);
                std::stringstream ss;
                ss << layer;
                if (focusLock.HasPrimaryFocus(layer)) {
                    Logf("Event binding %s: (has primary focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else if (focusLock.HasFocus(layer)) {
                    Logf("Event binding %s: (has focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else {
                    Logf("Event binding %s: (no focus: %s)", ecs::ToString(lock, ent), ss.str());
                }
            } else {
                Logf("Event binding %s: (no focus layer)", ecs::ToString(lock, ent));
            }

            auto &bindings = ent.Get<ecs::EventBindings>(lock);
            for (auto &bindingName : bindings.GetBindingNames()) {
                auto list = bindings.Lookup(bindingName);
                Logf("    %s:%s", bindingName, list->empty() ? " none" : "");
                for (auto &target : *list) {
                    auto e = target.first.Get(lock);
                    if (e) {
                        Logf("      %s on %s", target.second, ecs::ToString(lock, e));
                    } else {
                        Logf("      %s on %s(missing)", target.second, target.first.Name());
                    }
                }
            }
        }
    }

    void GameLogic::PrintSignals(std::string entityName) {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>>();
        Logf("Signal outputs:");
        for (auto ent : lock.EntitiesWith<ecs::SignalOutput>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            auto &output = ent.Get<ecs::SignalOutput>(lock);
            auto &signals = output.GetSignals();

            Logf("  %s:%s", ecs::ToString(lock, ent), signals.empty() ? " none" : "");
            for (auto &[signalName, value] : signals) {
                Logf("    %s: %.2f", signalName, value);
            }
        }

        Logf("");
        Logf("Signal bindings:");
        for (auto ent : lock.EntitiesWith<ecs::SignalBindings>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            auto &bindings = ent.Get<ecs::SignalBindings>(lock);
            auto bindingNames = bindings.GetBindingNames();
            Logf("  %s:%s", ecs::ToString(lock, ent), bindingNames.empty() ? " none" : "");
            for (auto &bindingName : bindingNames) {
                auto list = bindings.Lookup(bindingName);
                std::stringstream ss;
                ss << bindingName << ": ";
                if (list->sources.empty()) {
                    ss << "none";
                } else {
                    ss << list->operation;
                }
                Logf("    %s", ss.str());
                for (auto &source : list->sources) {
                    auto e = source.first.Get(lock);
                    double value = ecs::SignalBindings::GetSignal(lock, e, source.second);
                    if (e) {
                        Logf("      %s on %s: %.2f", source.second, ecs::ToString(lock, e), value);
                    } else {
                        Logf("      %s on %s(missing): %.2f", source.second, source.first.Name(), value);
                    }
                }
            }
        }
    }
} // namespace sp
