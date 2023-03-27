#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalStructAccess.hh"
#include "ecs/StructFieldTypes.hh"

namespace sp::scripts {
    using namespace ecs;

    struct EventGateBySignal {
        std::string inputEvent, outputEvent;
        SignalExpression gateExpression;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (inputEvent.empty()) return;
            state.definition.events = {inputEvent};
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (outputEvent.empty()) continue;

                if (gateExpression.EvaluateEvent(lock, event.data) >= 0.5) {
                    event.name = outputEvent;
                    EventBindings::SendEvent(lock, ent, event);
                }
            }
        }
    };
    StructMetadata MetadataEventGateBySignal(typeid(EventGateBySignal),
        StructField::New("input_event", &EventGateBySignal::inputEvent),
        StructField::New("output_event", &EventGateBySignal::outputEvent),
        StructField::New("gate_expr", &EventGateBySignal::gateExpression));
    InternalScript<EventGateBySignal> eventGateBySignal("event_gate_by_signal", MetadataEventGateBySignal);

    struct CollaposeEvents {
        robin_hood::unordered_map<std::string, std::string> mapping;

        template<typename LockType>
        void updateEvents(ScriptState &state, LockType lock, Entity ent, chrono_clock::duration interval) {
            if (mapping.empty()) return;
            state.definition.events.clear();
            for (auto &map : mapping) {
                state.definition.events.emplace_back(map.first);
            }
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.

            robin_hood::unordered_map<std::string, std::optional<Event>> outputEvents;
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                auto it = mapping.find(event.name);
                if (it == mapping.end()) continue;

                outputEvents[event.name] = Event{it->second, event.source, event.data};
            }
            for (auto &[name, outputEvent] : outputEvents) {
                if (outputEvent) EventBindings::SendEvent(lock, ent, *outputEvent);
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateEvents(state, lock, ent, interval);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateEvents(state, lock, ent, interval);
        }
    };
    StructMetadata MetadataCollaposeEvents(typeid(CollaposeEvents), StructField::New(&CollaposeEvents::mapping));
    InternalScript<CollaposeEvents> collapseEvents("collapse_events", MetadataCollaposeEvents);
    InternalPhysicsScript<CollaposeEvents> physicsCollapseEvents("physics_collapse_events", MetadataCollaposeEvents);

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

    struct EventFromSignal {
        robin_hood::unordered_map<std::string, SignalExpression> outputs;

        template<typename LockType>
        void sendOutputEvents(ScriptState &state, LockType lock, Entity ent, chrono_clock::duration interval) {
            for (auto &[name, expr] : outputs) {
                EventBindings::SendEvent(lock, ent, Event{name, ent, expr.Evaluate(lock)});
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            sendOutputEvents(state, lock, ent, interval);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            sendOutputEvents(state, lock, ent, interval);
        }
    };
    StructMetadata MetadataEventFromSignal(typeid(EventFromSignal), StructField::New(&EventFromSignal::outputs));
    InternalScript<EventFromSignal> eventFromSignal("event_from_signal", MetadataEventFromSignal);
    InternalPhysicsScript<EventFromSignal> physicsEventFromSignal("physics_event_from_signal", MetadataEventFromSignal);

    struct ComponentFromEvent {
        std::vector<std::string> outputs;

        template<typename LockType>
        void updateComponentFromEvent(ScriptState &state, LockType lock, Entity ent) {
            if (outputs.empty()) return;
            state.definition.events.clear();
            for (auto &fieldPath : outputs) {
                size_t delimiter = fieldPath.find('.');
                if (delimiter == std::string::npos) continue;
                auto componentName = fieldPath.substr(0, delimiter);
                auto fieldName = fieldPath.substr(delimiter + 1);
                if (componentName.empty() || fieldName.empty()) continue;

                state.definition.events.emplace_back("/set/" + componentName + "." + fieldName);
            }
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (!sp::starts_with(event.name, "/set/")) {
                    Errorf("Unexpected event received by component_from_event: %s", event.name);
                    continue;
                }

                auto fieldPath = std::string_view(event.name).substr("/set/"s.size());
                size_t delimiter = fieldPath.find('.');
                if (delimiter == std::string_view::npos) {
                    Errorf("Unexpected event received by component_from_event: %s", event.name);
                    continue;
                }
                std::string componentName(fieldPath.substr(0, delimiter));
                auto comp = LookupComponent(componentName);
                if (!comp) {
                    Errorf("ComponentFromEvent unknown component: %s", componentName);
                    continue;
                }
                if (!comp->HasComponent(lock, ent)) continue;

                auto field = ecs::GetStructField(comp->metadata.type, fieldPath);
                if (!field) {
                    Errorf("ComponentFromEvent unknown component field: %s", fieldPath);
                    continue;
                }

                void *compPtr = comp->Access(lock, ent);
                Assertf(compPtr,
                    "ComponentFromEvent %s access returned null data: %s",
                    componentName,
                    ecs::ToString(lock, ent));
                std::visit(
                    [&](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;

                        if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                            ecs::WriteStructField(compPtr, *field, [&arg](double &value) {
                                value = (double)arg;
                            });
                        } else {
                            Errorf("ComponentFromEvent '%s' incompatible type: setting %s to %s",
                                event.name,
                                typeid(T).name(),
                                comp->metadata.type.name());
                        }
                    },
                    event.data);
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
