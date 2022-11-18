#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    std::array miscScripts = {
        InternalScript("edge_trigger",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                auto inputName = state.GetParam<std::string>("input_signal");
                auto outputName = state.GetParam<std::string>("output_event");
                auto upperThreshold = state.GetParam<double>("upper_threshold");
                auto lowerThreshold = state.GetParam<double>("lower_threshold");

                auto oldTriggered = state.GetParam<bool>("triggered");
                auto newTriggered = oldTriggered;
                auto value = SignalBindings::GetSignal(lock, ent, inputName);
                if (upperThreshold >= lowerThreshold) {
                    if (value >= upperThreshold && !oldTriggered) {
                        newTriggered = true;
                    } else if (value <= lowerThreshold && oldTriggered) {
                        newTriggered = false;
                    }
                } else {
                    if (value <= upperThreshold && !oldTriggered) {
                        newTriggered = true;
                    } else if (value >= lowerThreshold && oldTriggered) {
                        newTriggered = false;
                    }
                }
                if (newTriggered != oldTriggered) {
                    if (newTriggered != 0.0f) {
                        EventBindings::SendEvent(lock, ent, Event{outputName, ent, (double)newTriggered});
                    }
                    state.SetParam<bool>("triggered", newTriggered);
                }
            }),
        InternalScript(
            "model_spawner",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name != "/script/spawn") continue;

                    glm::vec3 position;
                    position.x = state.GetParam<double>("position_x");
                    position.y = state.GetParam<double>("position_y");
                    position.z = state.GetParam<double>("position_z");
                    Transform transform(position);

                    auto fullTargetName = state.GetParam<std::string>("relative_to");
                    ecs::Name targetName(fullTargetName, state.scope.prefix);
                    if (targetName) {
                        auto targetEntity = state.GetParam<EntityRef>("target_entity");
                        if (targetEntity.Name() != targetName) targetEntity = targetName;

                        auto target = targetEntity.Get(lock);
                        if (target) {
                            state.SetParam<EntityRef>("target_entity", targetEntity);

                            if (target.Has<TransformSnapshot>(lock)) {
                                transform = target.Get<TransformSnapshot>(lock) * transform;
                            }
                        }
                    }

                    auto modelName = state.GetParam<std::string>("model");
                    auto scene = state.scope.scene.lock();
                    Assert(scene, "Model spawner script must have valid scene");

                    std::thread([ent, transform, modelName, scene, scope = state.scope]() {
                        auto model = sp::Assets().LoadGltf(modelName);

                        auto lock = ecs::StartTransaction<AddRemove>();
                        if (ent.Has<SceneInfo>(lock)) {
                            auto newEntity = scene->NewRootEntity(lock, scene, ecs::SceneInfo::Priority::Scene);

                            newEntity.Set<TransformTree>(lock, transform);
                            newEntity.Set<TransformSnapshot>(lock, transform);
                            newEntity.Set<Renderable>(lock, modelName, model);
                            newEntity.Set<Physics>(lock, modelName, PhysicsGroup::World, true, 1.0f);
                            newEntity.Set<PhysicsJoints>(lock);
                            newEntity.Set<PhysicsQuery>(lock);
                            newEntity.Set<EventInput>(lock);
                            auto &script = newEntity.Set<Script>(lock);
                            auto &newState = script.AddOnTick(scope, "interactive_object");
                            newState.filterOnEvent = true;
                        }
                    }).detach();
                }
            },
            "/script/spawn"),
        InternalScript("rotate",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree>(lock)) {
                    glm::vec3 rotationAxis;
                    rotationAxis.x = state.GetParam<double>("axis_x");
                    rotationAxis.y = state.GetParam<double>("axis_y");
                    rotationAxis.z = state.GetParam<double>("axis_z");
                    auto rotationSpeedRpm = state.GetParam<double>("speed");

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto currentRotation = transform.pose.GetRotation();
                    transform.pose.SetRotation(glm::rotate(currentRotation,
                        (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                        rotationAxis));
                }
            }),
        InternalScript("emissive_from_signal",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<Renderable>(lock)) return;

                auto signalName = state.GetParam<std::string>("signal_name");
                auto signalValue = SignalBindings::GetSignal(lock, ent, signalName);
                ent.Get<Renderable>(lock).emissiveScale = signalValue;
            }),
    };
} // namespace sp::scripts
