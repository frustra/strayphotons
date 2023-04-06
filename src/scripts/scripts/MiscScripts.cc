#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/SignalStructAccess.hh"
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

        bool enableFalling = true;
        bool enableRising = true;
        std::optional<SignalExpression> eventValue;

        // Internal script state
        SignalExpression expr;
        std::optional<double> previousValue;

        template<typename LockType>
        void updateEdgeTrigger(ScriptState &state, LockType &entLock) {
            if (expr.expr != inputExpr) {
                expr = SignalExpression(inputExpr, state.scope);
                if (!previousValue) previousValue = expr.Evaluate(entLock);
            }

            auto value = expr.Evaluate(entLock);

            Event outputEvent;
            outputEvent.name = outputName;
            outputEvent.source = entLock.entity;
            if (eventValue) {
                outputEvent.data = eventValue->Evaluate(entLock);
            } else {
                outputEvent.data = value >= 0.5;
            }

            if (value >= 0.5 && *previousValue < 0.5) {
                if (enableRising) EventBindings::SendEvent(entLock, outputEvent);
            } else if (value < 0.5 && *previousValue >= 0.5) {
                if (enableFalling) EventBindings::SendEvent(entLock, outputEvent);
            }
            previousValue = value;
        }

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            updateEdgeTrigger(state, entLock);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            updateEdgeTrigger(state, entLock);
        }
    };
    StructMetadata MetadataEdgeTrigger(typeid(EdgeTrigger),
        StructField::New("input_expr", &EdgeTrigger::inputExpr),
        StructField::New("output_event", &EdgeTrigger::outputName),
        StructField::New("falling_edge", &EdgeTrigger::enableFalling),
        StructField::New("rising_edge", &EdgeTrigger::enableRising),
        StructField::New("init_value", &EdgeTrigger::previousValue),
        StructField::New("set_event_value", &EdgeTrigger::eventValue));
    InternalScript<EdgeTrigger> edgeTrigger("edge_trigger", MetadataEdgeTrigger);
    InternalPhysicsScript<EdgeTrigger> physicsEdgeTrigger("physics_edge_trigger", MetadataEdgeTrigger);

    struct ModelSpawner {
        EntityRef targetEntity;
        glm::vec3 position;
        std::string modelName;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            Transform relativeTransform;
            auto target = targetEntity.Get(entLock);
            if (target.Has<TransformSnapshot>(entLock)) {
                relativeTransform = target.Get<TransformSnapshot>(entLock);
            }

            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                if (event.name != "/script/spawn") continue;

                Transform transform(position);
                transform = relativeTransform * transform;

                GetSceneManager().QueueAction([ent = entLock.entity,
                                                  transform,
                                                  modelName = modelName,
                                                  scope = state.scope]() {
                    auto lock = ecs::StartTransaction<ecs::AddRemove>();
                    if (!ent.Has<ecs::SceneInfo>(lock)) return;
                    auto &sceneInfo = ent.Get<ecs::SceneInfo>(lock);
                    auto scene = sceneInfo.scene.Lock();
                    if (!scene) return;

                    auto newEntity = scene->NewRootEntity(lock, scene);

                    newEntity.Set<TransformTree>(lock, transform);
                    newEntity.Set<TransformSnapshot>(lock, transform);
                    newEntity.Set<Renderable>(lock, modelName, sp::Assets().LoadGltf(modelName));
                    newEntity.Set<Physics>(lock, modelName, PhysicsGroup::World, ecs::PhysicsActorType::Dynamic, 1.0f);
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

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<TransformTree>() || rotationAxis == glm::vec3(0) || rotationSpeedRpm == 0.0f) return;

            auto &transform = entLock.Get<TransformTree>();
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

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<TransformTree>()) return;

            auto targetEnt = targetEntityRef.Get(entLock);
            if (!targetEnt.Has<TransformTree>(entLock)) return;

            auto &transform = entLock.Get<TransformTree>();
            auto parent = transform.parent.Get(entLock);

            auto targetTF = targetEnt.Get<TransformTree>(entLock);
            auto relativeTF = targetTF.GetRelativeTransform(entLock, parent);

            auto targetForward = relativeTF.GetPosition() - transform.pose.GetPosition();
            if (targetForward.x == 0 && targetForward.z == 0) return;
            targetForward = glm::normalize(targetForward);

            auto currentUp = glm::vec3(0, 1, 0);
            auto upEnt = upEntityRef.Get(entLock);
            if (upEnt.Has<TransformTree>(entLock)) {
                currentUp = upEnt.Get<TransformTree>(entLock).GetRelativeTransform(entLock, parent).GetUp();
            }

            auto targetRight = glm::normalize(glm::cross(currentUp, targetForward));
            auto targetUp = glm::normalize(glm::cross(targetForward, targetRight));

            transform.pose.offset[0] = targetRight;
            transform.pose.offset[1] = targetUp;
            transform.pose.offset[2] = targetForward;
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

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            if (!entLock.Has<SignalOutput>()) return;

            glm::dvec3 chargeColor = {std::max(0.0, chargeSignalRed.Evaluate(entLock)),
                std::max(0.0, chargeSignalGreen.Evaluate(entLock)),
                std::max(0.0, chargeSignalBlue.Evaluate(entLock))};
            double chargePower = chargeColor.r + chargeColor.g + chargeColor.b;
            chargeLevel += chargePower;

            if (chargePower <= 0.0) discharging = true;

            glm::dvec3 outputColor;
            if (discharging) {
                outputColor = {std::max(0.0, outputPowerRed.Evaluate(entLock)),
                    std::max(0.0, outputPowerGreen.Evaluate(entLock)),
                    std::max(0.0, outputPowerBlue.Evaluate(entLock))};
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

            auto &signalOutput = entLock.Get<SignalOutput>();
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
        void updateComponentFromSignal(LockType entLock) {
            for (auto &[fieldPath, signalExpr] : mapping) {
                size_t delimiter = fieldPath.find('.');
                if (delimiter == std::string::npos) {
                    Errorf("ComponentFromSignal unknown component path: %s", fieldPath);
                    continue;
                }
                auto componentName = fieldPath.substr(0, delimiter);
                auto comp = LookupComponent(componentName);
                if (!comp) {
                    Errorf("ComponentFromSignal unknown component: %s", componentName);
                    continue;
                }
                if (!comp->HasComponent(entLock, entLock.entity)) continue;

                auto signalValue = signalExpr.Evaluate(entLock);

                auto field = ecs::GetStructField(comp->metadata.type, fieldPath);
                if (!field) {
                    Errorf("ComponentFromSignal unknown component field: %s", fieldPath);
                    continue;
                }

                void *compPtr = comp->Access((EntityLock<Optional<WriteAll>>)entLock);
                if (!compPtr) {
                    Errorf("ComponentFromSignal %s access returned null data: %s",
                        componentName,
                        ecs::ToString(entLock));
                    continue;
                }
                ecs::WriteStructField(compPtr, *field, [&signalValue](double &value) {
                    value = signalValue;
                });
            }
        }

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            updateComponentFromSignal(entLock);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            updateComponentFromSignal(entLock);
        }
    };
    StructMetadata MetadataComponentFromSignal(typeid(ComponentFromSignal),
        StructField::New(&ComponentFromSignal::mapping));
    InternalScript<ComponentFromSignal> componentFromSignal("component_from_signal", MetadataComponentFromSignal);
    InternalPhysicsScript<ComponentFromSignal> physicsComponentFromSignal("physics_component_from_signal",
        MetadataComponentFromSignal);

    struct DebounceSignal {
        size_t delayFrames = 1;
        size_t delayMs = 0;
        SignalExpression input;
        std::string output;

        std::optional<double> lastSignal;
        size_t frameCount = 0;

        template<typename LockType>
        void updateSignal(LockType &entLock, chrono_clock::duration interval) {
            if (!entLock.template Has<SignalOutput>() || output.empty()) return;

            auto &signalOutput = entLock.template Get<SignalOutput>();
            if (!lastSignal || !signalOutput.HasSignal(output)) {
                lastSignal = signalOutput.GetSignal(output);
                signalOutput.SetSignal(output, *lastSignal);
            }
            auto currentInput = input.Evaluate(entLock);
            if ((currentInput >= 0.5) == (*lastSignal >= 0.5)) {
                frameCount++;
            } else {
                frameCount = 0;
                lastSignal = currentInput;
            }
            if (frameCount >= std::max(delayFrames, (size_t)(std::chrono::milliseconds(delayMs) / interval))) {
                signalOutput.SetSignal(output, currentInput);
            }
        }

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            updateSignal(entLock, interval);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            updateSignal(entLock, interval);
        }
    };
    StructMetadata MetadataDebounceSignal(typeid(DebounceSignal),
        StructField::New("delay_frames", &DebounceSignal::delayFrames),
        StructField::New("delay_ms", &DebounceSignal::delayMs),
        StructField::New("input", &DebounceSignal::input),
        StructField::New("output", &DebounceSignal::output));
    InternalScript<DebounceSignal> debounceSignal("debounce", MetadataDebounceSignal);
    InternalPhysicsScript<DebounceSignal> physicsDebounceSignal("physics_debounce", MetadataDebounceSignal);

    struct TimerSignal {
        std::vector<std::string> names = {"timer"};

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(names.size());
            for (auto &name : names) {
                state.definition.events.emplace_back("/reset_timer/" + name);
            }
        }

        template<typename LockType>
        void updateTimer(ScriptState &state, LockType &entLock, chrono_clock::duration interval) {
            if (!entLock.template Has<SignalOutput>() || names.empty()) return;

            auto &signalOutput = entLock.template Get<SignalOutput>();
            for (auto &name : names) {
                double timerValue = SignalBindings::GetSignal(entLock, name);
                bool timerEnable = SignalBindings::GetSignal(entLock, name + "_enable") >= 0.5;
                if (timerEnable) {
                    timerValue += interval.count() / 1e9;
                    signalOutput.SetSignal(name, timerValue);
                }
            }

            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                double eventValue = std::visit(
                    [](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;

                        if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                            return (double)arg;
                        } else {
                            return 0.0;
                        }
                    },
                    event.data);

                if (!sp::starts_with(event.name, "/reset_timer/")) {
                    Errorf("Unexpected event received by timer: %s", event.name);
                    continue;
                }

                auto timerName = event.name.substr("/reset_timer/"s.size());
                if (timerName.empty() || !sp::contains(names, timerName)) {
                    Errorf("Unexpected event received by timer: %s", event.name);
                    continue;
                }

                signalOutput.SetSignal(timerName, eventValue);
            }
        }

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            updateTimer(state, entLock, interval);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            updateTimer(state, entLock, interval);
        }
    };
    StructMetadata MetadataTimerSignal(typeid(TimerSignal), StructField::New("names", &TimerSignal::names));
    InternalScript<TimerSignal> timerScript("timer", MetadataTimerSignal);
    InternalPhysicsScript<TimerSignal> physicsTimerScript("physics_timer", MetadataTimerSignal);
} // namespace sp::scripts
