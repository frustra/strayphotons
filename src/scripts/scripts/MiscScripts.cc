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

    template<typename T, typename... Events>
    struct FancyScript {
        FancyScript(const std::string &name, const StructMetadata &metadata, Events... events) {
            GetScriptDefinitions().scripts.emplace(name,
                ScriptDefinition{name,
                    {events...},
                    &metadata,
                    [](ScriptState &state) -> void * {
                        if (!state.userData.has_value()) {
                            state.userData.emplace<T>();
                        }
                        return std::any_cast<T>(&state.userData);
                    },
                    OnTickFunc(
                        [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                            T scriptData;
                            if (state.userData.has_value()) {
                                scriptData = std::any_cast<T>(state.userData);
                            }
                            scriptData.OnTick(state, lock, ent, interval);
                            state.userData = scriptData;
                        })});
        }
    };

    struct EdgeTrigger {
        std::string inputExpr;
        std::string outputName = "/script/edge_trigger";
        SignalExpression expr;
        double previousValue;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (expr.expr != inputExpr) {
                expr = SignalExpression(inputExpr, state.scope.prefix);
                previousValue = expr.Evaluate(lock);
            }

            auto value = expr.Evaluate(lock);
            if (value >= 0.5 && previousValue < 0.5) {
                EventBindings::SendEvent(lock, ent, Event{outputName, ent, true});
            } else if (value < 0.5 && previousValue >= 0.5) {
                EventBindings::SendEvent(lock, ent, Event{outputName, ent, false});
            }
            previousValue = value;
        }
    };
    StructMetadata MetadataEdgeTrigger(typeid(EdgeTrigger),
        StructField::New("input_expr", &EdgeTrigger::inputExpr),
        StructField::New("output_event", &EdgeTrigger::outputName));
    FancyScript<EdgeTrigger> edgeTrigger("edge_trigger", MetadataEdgeTrigger);

    std::array miscScripts = {
        InternalScript(
            "model_spawner",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                struct ScriptData {
                    EntityRef targetEntity;
                };

                ScriptData scriptData;
                if (state.userData.has_value()) {
                    scriptData = std::any_cast<ScriptData>(state.userData);
                } else {
                    auto fullTargetName = state.GetParam<std::string>("relative_to");
                    ecs::Name targetName(fullTargetName, state.scope.prefix);
                    if (!targetName) {
                        Errorf("Model spawner relative target name is invalid: %s", fullTargetName);
                        return;
                    }
                    scriptData.targetEntity = targetName;
                }

                Transform relativeTransform;
                auto target = scriptData.targetEntity.Get(lock);
                if (target.Has<TransformSnapshot>(lock)) {
                    relativeTransform = target.Get<TransformSnapshot>(lock);
                }

                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name != "/script/spawn") continue;

                    glm::vec3 position;
                    position.x = state.GetParam<double>("position_x");
                    position.y = state.GetParam<double>("position_y");
                    position.z = state.GetParam<double>("position_z");
                    Transform transform(position);
                    transform = relativeTransform * transform;

                    auto modelName = state.GetParam<std::string>("model");
                    auto scene = state.scope.scene.lock();
                    Assert(scene, "Model spawner script must have valid scene");

                    std::thread([ent, transform, modelName, scene, scope = state.scope]() {
                        auto model = sp::Assets().LoadGltf(modelName);

                        auto lock = ecs::StartTransaction<AddRemove>();
                        if (ent.Has<SceneInfo>(lock)) {
                            auto newEntity = scene->NewRootEntity(lock, scene);

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

                state.userData = scriptData;
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
