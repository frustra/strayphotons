#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    #include "physx/PhysxManager.hh"
#endif

#include <picojson/picojson.h>

namespace sp {
    CFunc<void> CFuncPrintDebug("printdebug", "Print some debug info about the player", []() {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::TransformSnapshot, ecs::CharacterController, ecs::PhysicsQuery>>();
        auto player = ecs::EntityWith<ecs::Name>(lock, ecs::Name("player", "player"));
        auto flatview = ecs::EntityWith<ecs::Name>(lock, ecs::Name("player", "flatview"));
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
                    if (userData->standingOn) Logf("Standing on: %s", ecs::ToString(lock, userData->standingOn));
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
            for (auto &subQuery : query.queries) {
                auto *raycastQuery = std::get_if<ecs::PhysicsQuery::Raycast>(&subQuery);
                if (raycastQuery && raycastQuery->result) {
                    auto lookingAt = raycastQuery->result->target;
                    if (lookingAt) {
                        Logf("Looking at: %s", ecs::ToString(lock, lookingAt));
                    } else {
                        Logf("Looking at: nothing");
                    }
                }
            }
        }
    });

    CFunc<std::string> CFuncJsonDump("jsondump", "Print out a json listing of an entity", [](std::string entityName) {
        auto lock = ecs::World.StartTransaction<ecs::ReadAll>();
        ecs::EntityRef ref = ecs::Name(entityName, ecs::Name());
        ecs::Entity entity = ref.Get(lock);
        if (!entity) {
            Errorf("Entity not found: %s", entityName);
            return;
        }

        ecs::EntityScope scope;
        if (entity.Has<ecs::SceneInfo>(lock)) {
            auto &sceneInfo = entity.Get<ecs::SceneInfo>(lock);
            scope.scene = sceneInfo.scene;
            auto scene = sceneInfo.scene.lock();
            if (scene) scope.prefix.scene = scene->name;
        }

        picojson::object components;
        if (entity.Has<ecs::Name>(lock)) {
            auto &name = entity.Get<ecs::Name>(lock);
            components["name"] = picojson::value(name.String());
        }
        ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
            if (comp.HasComponent(lock, entity)) {
                comp.SaveEntity(lock, scope, components[comp.name], entity);
            }
        });
        auto val = picojson::value(components);
        Logf("Entity %s:\n%s", ecs::ToString(lock, entity), val.serialize(true));
    });

    CFunc<void> CFuncPrintEvents("printevents", "Print out the current state of event queues", []() {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::EventInput, ecs::EventBindings, ecs::FocusLayer, ecs::FocusLock>>();

        auto &focusLock = lock.Get<ecs::FocusLock>();
        for (auto ent : lock.EntitiesWith<ecs::EventInput>()) {
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
                if (queue->Empty()) {
                    Logf("  %s: empty", eventName);
                } else {
                    Logf("  %s: %u events", eventName, queue->Size());
                }
            }
        }

        for (auto ent : lock.EntitiesWith<ecs::EventBindings>()) {
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
                for (auto &binding : *list) {
                    auto target = binding.target.Get(lock);
                    if (target) {
                        Logf("      %s on %s", binding.destQueue, ecs::ToString(lock, target));
                    } else {
                        Logf("      %s on %s(missing)", binding.destQueue, binding.target.Name().String());
                    }
                }
            }
        }
    });

    CFunc<void> CFuncPrintSignals("printsignals", "Print out the values and bindings of signals", []() {
        auto lock = ecs::World.StartTransaction<ecs::ReadSignalsLock>();
        Logf("Signal outputs:");
        for (auto ent : lock.EntitiesWith<ecs::SignalOutput>()) {
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
            auto &bindings = ent.Get<ecs::SignalBindings>(lock);
            auto bindingNames = bindings.GetBindingNames();
            Logf("  %s:%s", ecs::ToString(lock, ent), bindingNames.empty() ? " none" : "");
            for (auto &bindingName : bindingNames) {
                auto list = bindings.Lookup(bindingName);
                std::stringstream ss;
                if (list->sources.empty()) {
                    ss << "none";
                } else {
                    ss << list->operation;
                }
                Logf("    %s: %s", bindingName, ss.str());
                for (auto &source : list->sources) {
                    auto e = source.first.Get(lock);
                    double value = ecs::SignalBindings::GetSignal(lock, e, source.second);
                    if (e) {
                        Logf("      %s on %s: %.2f", source.second, ecs::ToString(lock, e), value);
                    } else {
                        Logf("      %s on %s(missing): %.2f", source.second, source.first.Name().String(), value);
                    }
                }
            }
        }
    });

    CFunc<std::string> CFuncPrintSignal("printsignal",
        "Print out the value and bindings of a specific signal",
        [](std::string signalStr) {
            auto lock = ecs::World.StartTransaction<ecs::ReadSignalsLock>();

            auto [originName, signalName] = ecs::ParseSignalString(signalStr);
            if (!originName) {
                Errorf("Invalid signal name: %s", signalStr);
                return;
            }

            auto ent = ecs::EntityRef(originName).Get(lock);
            auto value = ecs::SignalBindings::GetSignal(lock, ent, signalName);
            Logf("%s/%s = %.2f", originName.String(), signalName, value);

            if (ent.Has<ecs::SignalOutput>(lock)) {
                auto &signalOutput = ent.Get<ecs::SignalOutput>(lock);
                if (signalOutput.HasSignal(signalName))
                    Logf("  Signal output: %.2f", signalOutput.GetSignal(signalName));
            }
            if (ent.Has<ecs::SignalBindings>(lock)) {
                auto &bindings = ent.Get<ecs::SignalBindings>(lock);
                auto bindingList = bindings.Lookup(signalName);

                std::stringstream ss;
                if (!bindingList || bindingList->sources.empty()) {
                    ss << "none";
                } else {
                    ss << bindingList->operation;
                }
                Logf("  Signal bindings: %s", ss.str());
                if (bindingList) {
                    for (auto &source : bindingList->sources) {
                        auto e = source.first.Get(lock);
                        double bindingValue = ecs::SignalBindings::GetSignal(lock, e, source.second);
                        if (e) {
                            Logf("      %s on %s: %.2f", source.second, ecs::ToString(lock, e), bindingValue);
                        } else {
                            Logf("      %s on %s(missing): %.2f",
                                source.second,
                                source.first.Name().String(),
                                bindingValue);
                        }
                    }
                }
            }
        });
} // namespace sp
