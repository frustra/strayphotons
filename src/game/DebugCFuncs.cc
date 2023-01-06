#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "game/SceneProperties.hh"

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    #include "physx/PhysxManager.hh"
#endif

#include <picojson/picojson.h>

namespace sp {
    CFunc<void> CFuncPrintDebug("printdebug", "Print some debug info about the player", []() {
        auto lock = ecs::StartTransaction<ecs::Read<ecs::Name,
            ecs::SceneInfo,
            ecs::TransformTree,
            ecs::TransformSnapshot,
            ecs::CharacterController,
            ecs::PhysicsQuery>>();
        auto player = entities::Player.Get(lock);
        auto head = entities::Head.Get(lock);

        SceneProperties sceneProperties = {};
        if (player.Has<ecs::SceneInfo>(lock)) {
            auto &properties = player.Get<ecs::SceneInfo>(lock).properties;
            if (properties) sceneProperties = *properties;
        }

        if (head.Has<ecs::TransformTree>(lock)) {
            auto &transform = head.Get<ecs::TransformTree>(lock);
            auto position = transform.GetGlobalTransform(lock).GetPosition();
            Logf("Head position: [%f, %f, %f]", position.x, position.y, position.z);
        }
        if (head.Has<ecs::TransformSnapshot>(lock)) {
            auto &transform = head.Get<ecs::TransformSnapshot>(lock);
            auto position = transform.GetPosition();
            Logf("Head position snapshot: [%f, %f, %f]", position.x, position.y, position.z);

            glm::vec3 gravityForce = sceneProperties.GetGravity(position);
            Logf("Gravity force: [%f, %f, %f]", gravityForce.x, gravityForce.y, gravityForce.z);
        }
        if (player.Has<ecs::TransformTree>(lock)) {
            auto &transform = player.Get<ecs::TransformTree>(lock);
            auto position = transform.GetGlobalTransform(lock).GetPosition();
            Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
        } else {
            Logf("Scene has no valid player");
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
                    Logf("Player position snapshot: [%f, %f, %f]", position.x, position.y, position.z);
                }
            } else {
                Logf("Player position snapshot: [%f, %f, %f]", position.x, position.y, position.z);
            }
#else
            Logf("Player position snapshot: [%f, %f, %f]", position.x, position.y, position.z);
#endif
        } else {
            Logf("Scene has no valid player snapshot");
        }

        auto flatview = entities::Flatview.Get(lock);
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
        auto lock = ecs::StartTransaction<ecs::ReadAll>();
        ecs::Entity entity;
        if (entityName.empty()) {
            auto flatview = entities::Flatview.Get(lock);
            if (flatview.Has<ecs::PhysicsQuery>(lock)) {
                auto &query = flatview.Get<ecs::PhysicsQuery>(lock);
                for (auto &subQuery : query.queries) {
                    auto *raycastQuery = std::get_if<ecs::PhysicsQuery::Raycast>(&subQuery);
                    if (raycastQuery && raycastQuery->result) {
                        entity = raycastQuery->result->target;
                        if (entity) break;
                    }
                }
            }
        } else {
            ecs::EntityRef ref = ecs::Name(entityName, ecs::Name());
            entity = ref.Get(lock);
        }
        if (!entity) {
            Errorf("Entity not found: %s", entityName);
            return;
        }

        ecs::EntityScope scope;
        if (entity.Has<ecs::SceneInfo>(lock)) {
            auto &sceneInfo = entity.Get<ecs::SceneInfo>(lock);
            if (sceneInfo.scene) scope.scene = sceneInfo.scene.name;
        }

        picojson::object components;
        if (entity.Has<ecs::Name>(lock)) {
            auto &name = entity.Get<ecs::Name>(lock);
            components["name"] = picojson::value(name.String());
        }
        ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
            if (comp.HasComponent(lock, entity)) {
                if (comp.metadata.fields.empty()) {
                    components[comp.name].set<picojson::object>({});
                }
                comp.SaveEntity(lock, scope, components[comp.name], entity);
            }
        });
        auto val = picojson::value(components);
        Logf("Entity %s:\n%s", ecs::ToString(lock, entity), val.serialize(true));
    });

    CFunc<std::string> CFuncSaveScene("savescene",
        "Print out a json serialization of the specified staging scene",
        [](std::string sceneName) {
            GetSceneManager().QueueActionAndBlock(SceneAction::SaveStagingScene, sceneName);
        });

    CFunc<void> CFuncPrintEvents("printevents", "Print out the current state of event queues", []() {
        auto lock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::EventInput, ecs::EventBindings>>();

        for (auto ent : lock.EntitiesWith<ecs::EventInput>()) {
            Logf("Event input %s:", ecs::ToString(lock, ent));

            auto &input = ent.Get<ecs::EventInput>(lock);
            for (auto &[eventName, queues] : input.events) {
                for (auto &queue : queues) {
                    if (queue->Empty()) {
                        Logf("  %s: empty", eventName);
                    } else {
                        Logf("  %s: %u events", eventName, queue->Size());
                    }
                }
            }
        }

        for (auto ent : lock.EntitiesWith<ecs::EventBindings>()) {
            Logf("Event binding %s:", ecs::ToString(lock, ent));

            auto &bindings = ent.Get<ecs::EventBindings>(lock);
            for (auto &[bindingName, list] : bindings.sourceToDest) {
                Logf("    %s:%s", bindingName, list.empty() ? " none" : "");
                for (auto &binding : list) {
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
        auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();
        Logf("Signals:");
        static const auto printEntity = [&lock](ecs::Entity ent) {
            Logf("  %s:", ecs::ToString(lock, ent));
            if (ent.Has<ecs::SignalOutput>(lock)) {
                auto &output = ent.Get<ecs::SignalOutput>(lock);
                for (auto &signal : output.signals) {
                    Logf("    %s = %.4f", signal.first, signal.second);
                }
            }
            if (ent.Has<ecs::SignalBindings>(lock)) {
                auto &bindings = ent.Get<ecs::SignalBindings>(lock);
                for (auto &pair : bindings.bindings) {
                    auto &binding = pair.second;
                    if (binding.nodes.empty() || binding.rootIndex < 0) {
                        Logf("    %s = nil", pair.first);
                    } else {
                        Logf("    %s = %.4f = %s",
                            pair.first,
                            binding.Evaluate(lock),
                            binding.nodeDebug[binding.rootIndex]);
                    }
                }
            }
        };
        for (auto ent : lock.EntitiesWith<ecs::SignalOutput>()) {
            printEntity(ent);
        }
        for (auto ent : lock.EntitiesWith<ecs::SignalBindings>()) {
            if (!ent.Has<ecs::SignalOutput>(lock)) printEntity(ent);
        }
    });

    CFunc<std::string> CFuncPrintSignal("printsignal",
        "Print out the value and bindings of a specific signal",
        [](std::string signalStr) {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

            auto [originName, signalName] = ecs::ParseSignalString(signalStr);
            if (!originName) {
                Errorf("Invalid signal name: %s", signalStr);
                return;
            }

            auto ent = ecs::EntityRef(originName).Get(lock);
            auto value = ecs::SignalBindings::GetSignal(lock, ent, signalName);
            Logf("%s/%s = %.4f", originName.String(), signalName, value);

            if (ent.Has<ecs::SignalOutput>(lock)) {
                auto &signalOutput = ent.Get<ecs::SignalOutput>(lock);
                if (signalOutput.HasSignal(signalName)) {
                    Logf("  Signal output: %.4f", signalOutput.GetSignal(signalName));
                }
            }
            if (ent.Has<ecs::SignalBindings>(lock)) {
                auto &bindings = ent.Get<ecs::SignalBindings>(lock);
                if (bindings.HasBinding(signalName)) {
                    auto &binding = bindings.GetBinding(signalName);
                    if (binding.nodes.empty() || binding.rootIndex < 0) {
                        Logf("  Signal binding: nil");
                    } else {
                        Logf("  Signal binding: %.4f = %s",
                            binding.Evaluate(lock),
                            binding.nodeDebug[binding.rootIndex]);
                    }
                }
            }
        });
} // namespace sp
