/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
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
        SignalExpression inputExpr;
        std::string outputName = "/script/edge_trigger";

        bool enableFalling = true;
        bool enableRising = true;
        std::optional<SignalExpression> eventValue;

        // Internal script state
        std::optional<double> previousValue;

        template<typename LockType>
        void updateEdgeTrigger(ScriptState &state, const LockType &lock, const Entity &ent) {
            auto value = inputExpr.Evaluate(lock);
            if (!previousValue) previousValue = value;

            Event outputEvent;
            outputEvent.name = outputName;
            outputEvent.source = ent;
            if (eventValue) {
                outputEvent.data = eventValue->Evaluate(lock);
            } else {
                outputEvent.data = value >= 0.5;
            }

            if (value >= 0.5 && *previousValue < 0.5) {
                if (enableRising) EventBindings::SendEvent(lock, ent, outputEvent);
            } else if (value < 0.5 && *previousValue >= 0.5) {
                if (enableFalling) EventBindings::SendEvent(lock, ent, outputEvent);
            }
            previousValue = value;
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateEdgeTrigger(state, lock, ent);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateEdgeTrigger(state, lock, ent);
        }
    };
    StructMetadata MetadataEdgeTrigger(typeid(EdgeTrigger),
        "EdgeTrigger",
        "",
        StructField::New("input_expr", &EdgeTrigger::inputExpr),
        StructField::New("output_event", &EdgeTrigger::outputName),
        StructField::New("falling_edge", &EdgeTrigger::enableFalling),
        StructField::New("rising_edge", &EdgeTrigger::enableRising),
        StructField::New("init_value", &EdgeTrigger::previousValue),
        StructField::New("set_event_value", &EdgeTrigger::eventValue),
        StructField::New("_previous_value", &EdgeTrigger::previousValue));
    InternalScript<EdgeTrigger> edgeTrigger("edge_trigger", MetadataEdgeTrigger);
    InternalPhysicsScript<EdgeTrigger> physicsEdgeTrigger("physics_edge_trigger", MetadataEdgeTrigger);

    struct ModelSpawner {
        EntityRef targetEntity;
        glm::vec3 position = glm::vec3(0);
        std::string modelName;
        std::vector<std::string> templates = {"interactive"};

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<Name, SceneInfo>(lock)) return;
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

                Entity stagingRootId = ent.Get<SceneInfo>(lock).rootStagingId;

                GetSceneManager().QueueAction(SceneAction::ApplySystemScene,
                    state.scope.scene,
                    [stagingRootId,
                        transform,
                        modelName = modelName,
                        templates = templates,
                        scriptId = state.GetInstanceId(),
                        scope = ent.Get<Name>(lock)](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                        auto newEntity = scene->NewPrefabEntity(lock, stagingRootId, scriptId, "", scope);

                        newEntity.Set<TransformTree>(lock, transform);
                        newEntity.Set<TransformSnapshot>(lock, transform);
                        if (!modelName.empty()) {
                            Renderable newRenderable = LookupComponent<Renderable>().StagingDefault();
                            newRenderable.modelName = modelName;
                            newRenderable.model = sp::Assets().LoadGltf(modelName);
                            newRenderable.meshIndex = 0;
                            newEntity.Set<Renderable>(lock, newRenderable);

                            Physics newPhysics = LookupComponent<Physics>().StagingDefault();
                            newPhysics.shapes = {PhysicsShape::ConvexMesh(modelName)};
                            newEntity.Set<Physics>(lock, newPhysics);
                        }
                        for (auto &templateName : templates) {
                            auto &scripts = newEntity.Get<Scripts>(lock);
                            auto &prefab = scripts.AddPrefab(scope, "template");
                            prefab.SetParam("source", templateName);
                        }
                        if (!templates.empty()) {
                            ecs::GetScriptManager().RunPrefabs(lock, newEntity);
                        }
                    });
            }
        }
    };
    StructMetadata MetadataModelSpawner(typeid(ModelSpawner),
        "ModelSpawner",
        "",
        StructField::New("relative_to", &ModelSpawner::targetEntity),
        StructField::New("position", &ModelSpawner::position),
        StructField::New("model", &ModelSpawner::modelName),
        StructField::New("templates", &ModelSpawner::templates));
    InternalScript<ModelSpawner> modelSpawner("model_spawner", MetadataModelSpawner, true, "/script/spawn");

    struct Rotate {
        glm::vec3 rotationAxis = glm::vec3(0);
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
        "Rotate",
        "",
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

            auto targetForward = relativeTF.GetPosition() - transform.pose.GetPosition();
            if (targetForward.x == 0 && targetForward.z == 0) return;
            targetForward = glm::normalize(targetForward);

            auto currentUp = glm::vec3(0, 1, 0);
            auto upEnt = upEntityRef.Get(lock);
            if (upEnt.Has<TransformTree>(lock)) {
                currentUp = upEnt.Get<TransformTree>(lock).GetRelativeTransform(lock, parent).GetUp();
            }

            auto targetRight = glm::normalize(glm::cross(currentUp, targetForward));
            auto targetUp = glm::normalize(glm::cross(targetForward, targetRight));

            transform.pose.offset[0] = targetRight;
            transform.pose.offset[1] = targetUp;
            transform.pose.offset[2] = targetForward;
        }
    };
    StructMetadata MetadataRotateToEntity(typeid(RotateToEntity),
        "RotateToEntity",
        "",
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
            glm::dvec3 chargeColor = {std::max(0.0, chargeSignalRed.Evaluate(lock)),
                std::max(0.0, chargeSignalGreen.Evaluate(lock)),
                std::max(0.0, chargeSignalBlue.Evaluate(lock))};
            double chargePower = chargeColor.r + chargeColor.g + chargeColor.b;
            chargeLevel += chargePower;

            if (chargePower <= 0.0) discharging = true;

            glm::dvec3 outputColor = glm::dvec3(0.0);
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

            SignalRef(ent, "discharging").SetValue(lock, discharging);
            SignalRef(ent, "charge_level").SetValue(lock, chargeLevel);
            SignalRef(ent, "max_charge_level").SetValue(lock, maxChargeLevel);
            SignalRef(ent, "cell_output_r").SetValue(lock, outputColor.r);
            SignalRef(ent, "cell_output_g").SetValue(lock, outputColor.g);
            SignalRef(ent, "cell_output_b").SetValue(lock, outputColor.b);
        }
    };
    StructMetadata MetadataChargeCell(typeid(ChargeCell),
        "ChargeCell",
        "",
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
        robin_hood::unordered_map<std::string, std::pair<const ComponentBase *, std::optional<StructField>>>
            componentCache;

        template<typename LockType>
        void updateComponentFromSignal(const LockType &lock, Entity ent) {
            for (auto &[fieldPath, signalExpr] : mapping) {
                size_t delimiter = fieldPath.find('.');
                if (delimiter == std::string::npos) {
                    Errorf("ComponentFromSignal unknown component path: %s", fieldPath);
                    continue;
                }
                auto componentName = fieldPath.substr(0, delimiter);

                const ecs::ComponentBase *comp;
                std::optional<StructField> field;
                auto existing = componentCache.find(fieldPath);
                if (existing != componentCache.end()) {
                    std::tie(comp, field) = existing->second;
                } else {
                    comp = LookupComponent(componentName);
                    if (!comp) {
                        Errorf("ComponentFromSignal unknown component: %s", componentName);
                        continue;
                    }
                    if (!comp->HasComponent(lock, ent)) continue;

                    field = ecs::GetStructField(comp->metadata.type, fieldPath);
                    if (!field) {
                        Errorf("ComponentFromSignal unknown component field: %s", fieldPath);
                        continue;
                    }

                    componentCache.emplace(fieldPath, std::pair{comp, field});
                }

                auto signalValue = signalExpr.Evaluate(lock);

                void *compPtr = comp->Access(lock, ent);
                if (!compPtr) {
                    Errorf("ComponentFromSignal %s access returned null data: %s",
                        componentName,
                        ecs::ToString(lock, ent));
                    continue;
                }
                ecs::WriteStructField(compPtr, *field, [&signalValue](double &value) {
                    value = signalValue;
                });
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
        "ComponentFromSignal",
        "",
        StructField::New(&ComponentFromSignal::mapping));
    InternalScript<ComponentFromSignal> componentFromSignal("component_from_signal", MetadataComponentFromSignal);
    InternalPhysicsScript<ComponentFromSignal> physicsComponentFromSignal("physics_component_from_signal",
        MetadataComponentFromSignal);

    struct SignalFromSignal {
        robin_hood::unordered_map<std::string, SignalExpression> mapping;
        std::vector<std::pair<SignalRef, const SignalExpression *>> refs;

        void Init(ScriptState &state) {
            refs.reserve(mapping.size());
        }

        template<typename LockType>
        void updateSignalFromSignal(const LockType &lock, Entity ent) {
            if (refs.size() < mapping.size()) {
                for (auto &[outputSignal, signalExpr] : mapping) {
                    refs.emplace_back(SignalRef(ent, outputSignal), &signalExpr);
                }
            }
            DynamicLock<ReadSignalsLock> readLock = lock.ReadOnlySubset();
            for (auto &[outputSignal, signalExpr] : refs) {
                outputSignal.SetValue(lock, signalExpr->Evaluate(readLock));
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateSignalFromSignal(lock, ent);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateSignalFromSignal(lock, ent);
        }
    };
    StructMetadata MetadataSignalFromSignal(typeid(SignalFromSignal),
        "SignalFromSignal",
        "",
        StructField::New(&SignalFromSignal::mapping));
    InternalScript<SignalFromSignal> signalFromSignal("signal_from_signal", MetadataSignalFromSignal);
    InternalPhysicsScript<SignalFromSignal> physicsSignalFromSignal("physics_signal_from_signal",
        MetadataSignalFromSignal);

    struct DebounceSignal {
        size_t delayFrames = 1;
        size_t delayMs = 0;
        SignalExpression input;
        std::string output;

        std::optional<double> lastSignal;
        size_t frameCount = 0;

        template<typename LockType>
        void updateSignal(const LockType &lock, Entity ent, chrono_clock::duration interval) {
            if (output.empty()) return;

            SignalRef ref(ent, output);
            if (!lastSignal || !ref.HasValue(lock)) {
                lastSignal = ref.GetSignal(lock);
                ref.SetValue(lock, *lastSignal);
            }
            auto currentInput = input.Evaluate(lock);
            if ((currentInput >= 0.5) == (*lastSignal >= 0.5)) {
                frameCount++;
            } else {
                frameCount = 0;
                lastSignal = currentInput;
            }
            if (frameCount >= std::max(delayFrames, (size_t)(std::chrono::milliseconds(delayMs) / interval))) {
                ref.SetValue(lock, currentInput);
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateSignal(lock, ent, interval);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateSignal(lock, ent, interval);
        }
    };
    StructMetadata MetadataDebounceSignal(typeid(DebounceSignal),
        "DebounceSignal",
        "",
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
        void updateTimer(ScriptState &state, LockType lock, Entity ent, chrono_clock::duration interval) {

            for (auto &name : names) {
                SignalRef valueRef(ent, name);
                double timerValue = valueRef.GetSignal(lock);
                bool timerEnable = SignalRef(ent, name + "_enable").GetSignal(lock) >= 0.5;
                if (timerEnable) {
                    timerValue += interval.count() / 1e9;
                    valueRef.SetValue(lock, timerValue);
                }
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
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

                SignalRef(ent, timerName).SetValue(lock, eventValue);
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateTimer(state, lock, ent, interval);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateTimer(state, lock, ent, interval);
        }
    };
    StructMetadata MetadataTimerSignal(typeid(TimerSignal),
        "TimerSignal",
        "",
        StructField::New("names", &TimerSignal::names));
    InternalScript<TimerSignal> timerScript("timer", MetadataTimerSignal);
    InternalPhysicsScript<TimerSignal> physicsTimerScript("physics_timer", MetadataTimerSignal);
} // namespace sp::scripts
