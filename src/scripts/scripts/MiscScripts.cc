#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/StructFieldTypes.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct EdgeTrigger {
        // Input parameters
        std::string inputExpr;
        std::string outputName = "/script/edge_trigger";

        // Internal script state
        SignalExpression expr;
        double previousValue;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (expr.expr != inputExpr) {
                expr = SignalExpression(inputExpr, state.scope);
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
    InternalScript<EdgeTrigger> edgeTrigger("edge_trigger", MetadataEdgeTrigger);

    struct ModelSpawner {
        EntityRef targetEntity;
        glm::vec3 position;
        std::string modelName;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            Transform relativeTransform;
            auto target = targetEntity.Get(lock);
            if (target.Has<TransformSnapshot>(lock)) {
                relativeTransform = target.Get<TransformSnapshot>(lock);
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name != "/script/spawn") continue;

                Transform transform(position);
                transform = relativeTransform * transform;

                GetSceneManager().QueueAction([ent, transform, modelName = modelName, scope = state.scope]() {
                    auto lock = ecs::StartTransaction<ecs::AddRemove>();
                    if (!ent.Has<ecs::SceneInfo>(lock)) return;
                    auto &sceneInfo = ent.Get<ecs::SceneInfo>(lock);
                    auto scene = sceneInfo.scene.Lock();
                    if (!scene) return;

                    auto newEntity = scene->NewRootEntity(lock, scene);

                    newEntity.Set<TransformTree>(lock, transform);
                    newEntity.Set<TransformSnapshot>(lock, transform);
                    newEntity.Set<Renderable>(lock, modelName, sp::Assets().LoadGltf(modelName));
                    newEntity.Set<Physics>(lock, modelName, PhysicsGroup::World, true, 1.0f);
                    newEntity.Set<PhysicsJoints>(lock);
                    newEntity.Set<PhysicsQuery>(lock);
                    newEntity.Set<EventInput>(lock);
                    auto &scripts = newEntity.Set<Scripts>(lock);
                    scripts.AddOnTick(scope, "interactive_object");
                });
            }
        }
    };
    StructMetadata MetadataModelSpawner(typeid(ModelSpawner),
        StructField::New("relative_to", &ModelSpawner::targetEntity),
        StructField::New("position", &ModelSpawner::position),
        StructField::New("model", &ModelSpawner::modelName));
    InternalScript<ModelSpawner> modelSpawner("model_spawner", MetadataModelSpawner, true, "/script/spawn");

    struct Rotate {
        glm::vec3 rotationAxis;
        float rotationSpeedRpm;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree>(lock) || rotationAxis == glm::vec3(0) || rotationSpeedRpm == 0.0f) return;

            auto &transform = ent.Get<TransformTree>(lock);
            auto currentRotation = transform.pose.GetRotation();
            transform.pose.SetRotation(glm::rotate(currentRotation,
                (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                rotationAxis));
        }
    };
    StructMetadata MetadataRotate(typeid(Rotate),
        StructField::New("axis", &Rotate::rotationAxis),
        StructField::New("speed", &Rotate::rotationSpeedRpm));
    InternalScript<Rotate> rotate("rotate", MetadataRotate);

    struct RotateToEntity {
        EntityRef targetEntityRef, upEntityRef;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree>(lock)) return;

            auto targetEnt = targetEntityRef.Get(lock);
            if (!targetEnt.Has<TransformTree>(lock)) return;

            auto &transform = ent.Get<TransformTree>(lock);
            auto parent = transform.parent.Get(lock);

            auto targetTF = targetEnt.Get<TransformTree>(lock);
            auto relativeTF = targetTF.GetRelativeTransform(lock, parent);

            auto targetForward = transform.pose.GetPosition() - relativeTF.GetPosition();
            if (targetForward.x == 0 && targetForward.z == 0) return;
            targetForward = glm::normalize(targetForward);

            auto currentUp = glm::vec3(0, 1, 0);
            auto upEnt = upEntityRef.Get(lock);
            if (upEnt.Has<TransformTree>(lock)) {
                currentUp = upEnt.Get<TransformTree>(lock).GetRelativeTransform(lock, parent).GetUp();
            }

            auto currentScale = transform.pose.GetScale();
            auto targetRight = glm::normalize(glm::cross(targetForward, currentUp));
            auto targetUp = glm::normalize(glm::cross(targetRight, targetForward));

            transform.pose.matrix[0] = targetRight * currentScale.x;
            transform.pose.matrix[1] = targetUp * currentScale.y;
            transform.pose.matrix[2] = targetForward * currentScale.z;
        }
    };
    StructMetadata MetadataRotateToEntity(typeid(RotateToEntity),
        StructField::New("up", &RotateToEntity::upEntityRef),
        StructField::New("target", &RotateToEntity::targetEntityRef));
    InternalScript<RotateToEntity> rotateToEntity("rotate_to_entity", MetadataRotateToEntity);

    struct ChargeCell {
        // Charge level is light units * ticks
        double chargeLevel = 0.0;
        double maxChargeLevel = 1.0;
        SignalExpression outputPowerRed;
        SignalExpression outputPowerGreen;
        SignalExpression outputPowerBlue;
        SignalExpression chargeSignalRed;
        SignalExpression chargeSignalGreen;
        SignalExpression chargeSignalBlue;
        bool discharging = false;

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<SignalOutput>(lock)) return;

            glm::dvec3 chargeColor = {std::max(0.0, chargeSignalRed.Evaluate(lock)),
                std::max(0.0, chargeSignalGreen.Evaluate(lock)),
                std::max(0.0, chargeSignalBlue.Evaluate(lock))};
            double chargePower = chargeColor.r + chargeColor.g + chargeColor.b;
            chargeLevel += chargePower;

            if (chargePower <= 0.0) discharging = true;

            glm::dvec3 outputColor;
            if (discharging) {
                outputColor = {std::max(0.0, outputPowerRed.Evaluate(lock)),
                    std::max(0.0, outputPowerGreen.Evaluate(lock)),
                    std::max(0.0, outputPowerBlue.Evaluate(lock))};
                double outputPower = outputColor.r + outputColor.g + outputColor.b;
                if (outputPower > 0 && discharging) {
                    if (outputPower >= chargeLevel) {
                        outputColor *= chargeLevel / outputPower;
                        chargeLevel = 0;
                        discharging = false;
                    } else {
                        chargeLevel -= outputPower;
                    }
                }
            }

            chargeLevel = std::clamp(chargeLevel, 0.0, maxChargeLevel);

            auto &signalOutput = ent.Get<SignalOutput>(lock);
            signalOutput.SetSignal("discharging", discharging);
            signalOutput.SetSignal("charge_level", chargeLevel);
            signalOutput.SetSignal("max_charge_level", maxChargeLevel);
            signalOutput.SetSignal("cell_output_r", outputColor.r);
            signalOutput.SetSignal("cell_output_g", outputColor.g);
            signalOutput.SetSignal("cell_output_b", outputColor.b);
        }
    };
    StructMetadata MetadataChargeCell(typeid(ChargeCell),
        StructField::New("discharging", &ChargeCell::discharging),
        StructField::New("charge_level", &ChargeCell::chargeLevel),
        StructField::New("max_charge_level", &ChargeCell::maxChargeLevel),
        StructField::New("output_power_r", &ChargeCell::outputPowerRed),
        StructField::New("output_power_g", &ChargeCell::outputPowerGreen),
        StructField::New("output_power_b", &ChargeCell::outputPowerBlue),
        StructField::New("charge_signal_r", &ChargeCell::chargeSignalRed),
        StructField::New("charge_signal_g", &ChargeCell::chargeSignalGreen),
        StructField::New("charge_signal_b", &ChargeCell::chargeSignalBlue));
    InternalPhysicsScript<ChargeCell> chargeCell("charge_cell", MetadataChargeCell);

    struct ComponentFromSignal {
        robin_hood::unordered_map<std::string, SignalExpression> mapping;

        template<typename LockType>
        void updateComponentFromSignal(LockType lock, Entity ent) {
            for (auto &[fieldPath, signalExpr] : mapping) {
                size_t delimiter = fieldPath.find('#');
                if (delimiter == std::string::npos) continue;
                auto componentName = fieldPath.substr(0, delimiter);
                auto fieldName = fieldPath.substr(delimiter + 1);
                auto comp = LookupComponent(componentName);
                if (!comp) {
                    Errorf("ComponentFromSignal unknown component: %s", componentName);
                    continue;
                }
                if (!comp->HasComponent(lock, ent)) continue;
                auto &metadata = comp->metadata;

                auto signalValue = signalExpr.Evaluate(lock);

                void *compPtr = comp->Access(lock, ent);
                Assertf(compPtr,
                    "ComponentFromSignal %s access returned null data: %s",
                    componentName,
                    ecs::ToString(lock, ent));
                for (auto &field : metadata.fields) {
                    if (starts_with(fieldName, field.name)) {
                        std::string_view subField;
                        if (fieldName.length() > field.name.length()) {
                            if (fieldName[field.name.length()] != '.') continue;
                            subField = std::string_view(fieldName).substr(field.name.length() + 1);
                        }
                        ecs::GetFieldType(field.type, [&](auto *typePtr) {
                            using T = std::remove_pointer_t<decltype(typePtr)>;
                            if constexpr (is_glm_vec<T>::value || std::is_same_v<T, color_t> ||
                                          std::is_same_v<T, color_alpha_t>) {
                                if (subField.empty()) {
                                    auto &vec = *field.Access<T>(compPtr);
                                    for (int i = 0; i < T::length(); i++) {
                                        vec[i] = signalValue;
                                    }
                                } else if (subField.length() == 1) {
                                    static const std::array<std::string, 3> indexChars = {"xyzw", "rgba", "0123"};
                                    bool found = false;
                                    for (auto &chars : indexChars) {
                                        auto index = std::find(chars.begin(), chars.end(), subField[0]) - chars.begin();
                                        if (index >= 0 && index < T::length()) {
                                            (*field.Access<T>(compPtr))[index] = signalValue;
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        Errorf("Invalid glm::vec subfield: %s", subField);
                                    }
                                } else {
                                    Errorf("Invalid glm::vec subfield: %s", subField);
                                }
                            } else if constexpr (std::is_convertible_v<double, T>) {
                                *field.Access<T>(compPtr) = signalValue;
                            } else {
                                Errorf("ComponentFromSignal '%s#%s' unsupported type conversion: double to %s",
                                    componentName,
                                    fieldName,
                                    typeid(T).name());
                            }
                        });
                        break;
                    }
                }
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateComponentFromSignal(lock, ent);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateComponentFromSignal(lock, ent);
        }
    };
    StructMetadata MetadataComponentFromSignal(typeid(ComponentFromSignal),
        StructField::New(&ComponentFromSignal::mapping));
    InternalScript<ComponentFromSignal> componentFromSignal("component_from_signal", MetadataComponentFromSignal);
    InternalPhysicsScript<ComponentFromSignal> physicsComponentFromSignal("physics_component_from_signal",
        MetadataComponentFromSignal);
} // namespace sp::scripts
