#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalStructAccess.hh"

namespace sp::scripts {
    using namespace ecs;

    struct InitEvent {
        EntityRef target;
        std::vector<std::string> outputs;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            state.definition.events.clear();
            state.definition.filterOnEvent = true; // Effective next tick, only executes once on first frame.

            for (auto &output : outputs) {
                EventBindings::SendEvent(entLock, Event{output, entLock.entity, true});
            }
        }
    };
    StructMetadata MetadataInitEvent(typeid(InitEvent), StructField::New(&InitEvent::outputs));
    InternalScript<InitEvent> initEvent("init_event", MetadataInitEvent);

    struct EventGateBySignal {
        std::string inputEvent, outputEvent;
        SignalExpression gateExpression;

        void Init(ScriptState &state) {
            if (inputEvent.empty()) {
                state.definition.events.clear();
            } else {
                state.definition.events = {inputEvent};
            }
            state.definition.filterOnEvent = true;
        }

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            ZoneScoped;
            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                if (outputEvent.empty()) continue;

                if (gateExpression.EvaluateEvent(entLock, event.data) >= 0.5) {
                    event.name = outputEvent;
                    EventBindings::SendEvent(entLock, event);
                }
            }
        }
    };
    StructMetadata MetadataEventGateBySignal(typeid(EventGateBySignal),
        StructField::New("input_event", &EventGateBySignal::inputEvent),
        StructField::New("output_event", &EventGateBySignal::outputEvent),
        StructField::New("gate_expr", &EventGateBySignal::gateExpression));
    InternalScript<EventGateBySignal> eventGateBySignal("event_gate_by_signal", MetadataEventGateBySignal);

    struct CollapseEvents {
        robin_hood::unordered_map<std::string, std::string> mapping;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(mapping.size());
            for (auto &map : mapping) {
                state.definition.events.emplace_back(map.first);
            }
            state.definition.filterOnEvent = true;
        }

        template<typename LockType>
        void updateEvents(ScriptState &state, LockType entLock, chrono_clock::duration interval) {
            ZoneScoped;
            robin_hood::unordered_map<std::string, std::optional<Event>> outputEvents;
            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                auto it = mapping.find(event.name);
                if (it == mapping.end()) continue;

                outputEvents[event.name] = Event{it->second, event.source, event.data};
            }
            for (auto &[name, outputEvent] : outputEvents) {
                if (outputEvent) EventBindings::SendEvent(entLock, *outputEvent);
            }
        }

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            updateEvents(state, entLock, interval);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            updateEvents(state, entLock, interval);
        }
    };
    StructMetadata MetadataCollapseEvents(typeid(CollapseEvents), StructField::New(&CollapseEvents::mapping));
    InternalScript<CollapseEvents> collapseEvents("collapse_events", MetadataCollapseEvents);
    InternalPhysicsScript<CollapseEvents> physicsCollapseEvents("physics_collapse_events", MetadataCollapseEvents);

    struct SignalFromEvent {
        std::vector<std::string> outputs;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(outputs.size() * 4);
            for (auto &outputSignal : outputs) {
                state.definition.events.emplace_back("/signal/toggle/" + outputSignal);
                state.definition.events.emplace_back("/signal/set/" + outputSignal);
                state.definition.events.emplace_back("/signal/add/" + outputSignal);
                state.definition.events.emplace_back("/signal/clear/" + outputSignal);
            }
            state.definition.filterOnEvent = true;
        }

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<SignalOutput>()) return;
            auto &signalOutput = entLock.Get<SignalOutput>();
            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                Assertf(sp::starts_with(event.name, "/signal/"), "Event name should be /signal/<action>/<signal>");
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

                auto eventName = std::string_view(event.name).substr("/signal/"s.size());
                auto delimiter = eventName.find('/');
                Assertf(delimiter != std::string_view::npos, "Event name should be /signal/<action>/<signal>");
                auto action = eventName.substr(0, delimiter);
                std::string signalName(eventName.substr(delimiter + 1));
                if (signalName.empty()) continue;

                if (action == "toggle") {
                    double currentValue = SignalBindings::GetSignal(entLock, signalName);

                    if (glm::epsilonEqual(currentValue, eventValue, (double)std::numeric_limits<float>::epsilon())) {
                        signalOutput.SetSignal(signalName, 0);
                    } else {
                        signalOutput.SetSignal(signalName, eventValue);
                    }
                } else if (action == "set") {
                    signalOutput.SetSignal(signalName, eventValue);
                } else if (action == "add") {
                    signalOutput.SetSignal(signalName, signalOutput.GetSignal(signalName) + eventValue);
                } else if (action == "clear") {
                    signalOutput.ClearSignal(signalName);
                } else {
                    Errorf("Unknown signal action: '%s'", std::string(action));
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
        void sendOutputEvents(ScriptState &state, LockType entLock, chrono_clock::duration interval) {
            for (auto &[name, expr] : outputs) {
                EventBindings::SendEvent(entLock, Event{name, entLock.entity, expr.Evaluate(entLock)});
            }
        }

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            sendOutputEvents(state, entLock, interval);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            sendOutputEvents(state, entLock, interval);
        }
    };
    StructMetadata MetadataEventFromSignal(typeid(EventFromSignal), StructField::New(&EventFromSignal::outputs));
    InternalScript<EventFromSignal> eventFromSignal("event_from_signal", MetadataEventFromSignal);
    InternalPhysicsScript<EventFromSignal> physicsEventFromSignal("physics_event_from_signal", MetadataEventFromSignal);

    struct ComponentFromEvent {
        std::vector<std::string> outputs;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(outputs.size());
            for (auto &fieldPath : outputs) {
                size_t delimiter = fieldPath.find('.');
                if (delimiter == std::string::npos) continue;
                auto componentName = fieldPath.substr(0, delimiter);
                auto fieldName = fieldPath.substr(delimiter + 1);
                if (componentName.empty() || fieldName.empty()) continue;

                state.definition.events.emplace_back("/set/" + componentName + "." + fieldName);
            }
            state.definition.filterOnEvent = true;
        }

        template<typename LockType>
        void updateComponentFromEvent(ScriptState &state, LockType entLock) {
            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
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
                if (!comp->HasComponent(entLock, entLock.entity)) continue;

                auto field = ecs::GetStructField(comp->metadata.type, fieldPath);
                if (!field) {
                    Errorf("ComponentFromEvent unknown component field: %s", fieldPath);
                    continue;
                }

                void *compPtr = comp->Access((EntityLock<Optional<WriteAll>>)entLock);
                Assertf(compPtr,
                    "ComponentFromEvent %s access returned null data: %s",
                    componentName,
                    ecs::ToString(entLock));
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

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            updateComponentFromEvent(state, entLock);
        }
        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            updateComponentFromEvent(state, entLock);
        }
    };
    StructMetadata MetadataComponentFromEvent(typeid(ComponentFromEvent),
        StructField::New("outputs", &ComponentFromEvent::outputs));
    InternalScript<ComponentFromEvent> componentFromEvent("component_from_event", MetadataComponentFromEvent);
    InternalPhysicsScript<ComponentFromEvent> physicsComponentFromEvent("physics_component_from_event",
        MetadataComponentFromEvent);
} // namespace sp::scripts
