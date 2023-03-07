#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/StructFieldTypes.hh"

namespace sp::scripts {
    using namespace ecs;

    struct EventGateBySignal {
        std::string inputEvent, outputEvent;
        SignalExpression gateExpression;
        float signalThreshold = 0.5f;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (inputEvent.empty()) return;
            state.definition.events = {inputEvent};
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.

            bool gateOpen = gateExpression.Evaluate(lock) >= signalThreshold;
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (!gateOpen || outputEvent.empty()) continue;

                event.name = outputEvent;
                EventBindings::SendEvent(lock, ent, event);
            }
        }
    };
    StructMetadata MetadataEventGateBySignal(typeid(EventGateBySignal),
        StructField::New("input_event", &EventGateBySignal::inputEvent),
        StructField::New("output_event", &EventGateBySignal::outputEvent),
        StructField::New("gate_signal", &EventGateBySignal::gateExpression),
        StructField::New("signal_threshold", &EventGateBySignal::signalThreshold));
    InternalScript<EventGateBySignal> eventGateBySignal("event_gate_by_signal", MetadataEventGateBySignal);

    struct SignalFromEvent {
        std::vector<std::string> outputs;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<SignalOutput>(lock) || outputs.empty()) return;
            state.definition.events.clear();
            for (auto &outputSignal : outputs) {
                state.definition.events.emplace_back("/toggle_signal/" + outputSignal);
                state.definition.events.emplace_back("/set_signal/" + outputSignal);
                state.definition.events.emplace_back("/clear_signal/" + outputSignal);
            }
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.

            auto &signalOutput = ent.Get<SignalOutput>(lock);
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                double eventValue = std::visit(
                    [](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;

                        if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                            return (double)arg;
                        } else if constexpr (std::is_convertible_v<T, bool>) {
                            return (double)(bool)arg;
                        } else {
                            return 1.0;
                        }
                    },
                    event.data);

                if (sp::starts_with(event.name, "/toggle_signal/")) {
                    auto signalName = event.name.substr("/toggle_signal/"s.size());
                    if (signalName.empty()) continue;
                    double currentValue = SignalBindings::GetSignal(lock, ent, signalName);

                    if (glm::epsilonEqual(currentValue, eventValue, (double)std::numeric_limits<float>::epsilon())) {
                        signalOutput.SetSignal(signalName, 0);
                    } else {
                        signalOutput.SetSignal(signalName, eventValue);
                    }
                } else if (sp::starts_with(event.name, "/set_signal/")) {
                    auto signalName = event.name.substr("/set_signal/"s.size());
                    if (signalName.empty()) continue;
                    signalOutput.SetSignal(signalName, eventValue);
                } else if (sp::starts_with(event.name, "/clear_signal/")) {
                    auto signalName = event.name.substr("/clear_signal/"s.size());
                    if (signalName.empty()) continue;
                    signalOutput.ClearSignal(signalName);
                } else {
                    Errorf("Unexpected event received by signal_from_event: %s", event.name);
                }
            }
        }
    };
    StructMetadata MetadataSignalFromEvent(typeid(SignalFromEvent),
        StructField::New("outputs", &SignalFromEvent::outputs));
    InternalScript<SignalFromEvent> signalFromEvent("signal_from_event", MetadataSignalFromEvent);

    struct ComponentFromEvent {
        std::vector<std::string> outputs;

        template<typename LockType>
        void updateComponentFromEvent(ScriptState &state, LockType lock, Entity ent) {
            if (outputs.empty()) return;
            state.definition.events.clear();
            for (auto &fieldPath : outputs) {
                size_t delimiter = fieldPath.find('#');
                if (delimiter == std::string::npos) continue;
                auto componentName = fieldPath.substr(0, delimiter);
                auto fieldName = fieldPath.substr(delimiter + 1);
                if (componentName.empty() || fieldName.empty()) continue;

                state.definition.events.emplace_back("/set/" + componentName + "/" + fieldName);
            }
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                double eventValue = std::visit(
                    [](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;

                        if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                            return (double)arg;
                        } else if constexpr (std::is_convertible_v<T, bool>) {
                            return (double)(bool)arg;
                        } else {
                            return 1.0;
                        }
                    },
                    event.data);

                if (!sp::starts_with(event.name, "/set/")) {
                    Errorf("Unexpected event received by component_from_event: %s", event.name);
                    continue;
                }

                size_t delimiter = event.name.find('/', "/set/"s.size());
                if (delimiter == std::string::npos) {
                    Errorf("Unexpected event received by component_from_event: %s", event.name);
                    continue;
                }
                auto componentName = event.name.substr("/set/"s.size(), delimiter);
                auto fieldName = event.name.substr(delimiter + 1);
                auto comp = LookupComponent(componentName);
                if (!comp) {
                    Errorf("ComponentFromEvent unknown component: %s", componentName);
                    continue;
                }
                if (!comp->HasComponent(lock, ent)) continue;
                auto &metadata = comp->metadata;

                void *compPtr = comp->Access(lock, ent);
                Assertf(compPtr,
                    "ComponentFromEvent %s access returned null data: %s",
                    componentName,
                    ecs::ToString(lock, ent));
                for (const StructField &field : metadata.fields) {
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
                                        vec[i] = eventValue;
                                    }
                                } else if (subField.length() == 1) {
                                    static const std::array<std::string, 3> indexChars = {"xyzw", "rgba", "0123"};
                                    bool found = false;
                                    for (auto &chars : indexChars) {
                                        auto index = std::find(chars.begin(), chars.end(), subField[0]) - chars.begin();
                                        if (index >= 0 && index < T::length()) {
                                            (*field.Access<T>(compPtr))[index] = eventValue;
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
                                *field.Access<T>(compPtr) = eventValue;
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
            updateComponentFromEvent(state, lock, ent);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateComponentFromEvent(state, lock, ent);
        }
    };
    StructMetadata MetadataComponentFromEvent(typeid(ComponentFromEvent),
        StructField::New("outputs", &ComponentFromEvent::outputs));
    InternalScript<ComponentFromEvent> componentFromEvent("component_from_event", MetadataComponentFromEvent);
    InternalPhysicsScript<ComponentFromEvent> physicsComponentFromEvent("physics_component_from_event",
        MetadataComponentFromEvent);
} // namespace sp::scripts
