/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalStructAccess.hh"

namespace sp::scripts {
    using namespace ecs;

    struct InitEvent {
        std::vector<std::string> outputs;

        void OnTick(ScriptState &state, SendEventsLock lock, Entity ent, chrono_clock::duration interval) {
            state.definition.events.clear();
            state.definition.filterOnEvent = true; // Effective next tick, only executes once on first frame.

            for (auto &output : outputs) {
                EventBindings::SendEvent(lock, ent, Event{output, ent, true});
            }
        }
    };
    StructMetadata MetadataInitEvent(typeid(InitEvent),
        sizeof(InitEvent),
        "InitEvent",
        "",
        StructField::New(&InitEvent::outputs));
    LogicScript<InitEvent> initEvent("init_event", MetadataInitEvent);

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

        void OnTick(ScriptState &state, SendEventsLock lock, Entity ent, chrono_clock::duration interval) {
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
        sizeof(EventGateBySignal),
        "EventGateBySignal",
        "",
        StructField::New("input_event", &EventGateBySignal::inputEvent),
        StructField::New("output_event", &EventGateBySignal::outputEvent),
        StructField::New("gate_expr", &EventGateBySignal::gateExpression));
    LogicScript<EventGateBySignal> eventGateBySignal("event_gate_by_signal", MetadataEventGateBySignal);

    struct CollapseEvents {
        robin_hood::unordered_map<EventName, std::string, StringHash, StringEqual> mapping;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(mapping.size());
            for (auto &map : mapping) {
                state.definition.events.emplace_back(map.first);
            }
            state.definition.filterOnEvent = true;
        }

        void OnTick(ScriptState &state, SendEventsLock lock, Entity ent, chrono_clock::duration interval) {
            robin_hood::unordered_map<EventName, std::optional<Event>, StringHash, StringEqual> outputEvents;
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
    };
    StructMetadata MetadataCollapseEvents(typeid(CollapseEvents),
        sizeof(CollapseEvents),
        "CollapseEvents",
        "",
        StructField::New(&CollapseEvents::mapping));
    LogicScript<CollapseEvents> collapseEvents("collapse_events", MetadataCollapseEvents);
    PhysicsScript<CollapseEvents> physicsCollapseEvents("physics_collapse_events", MetadataCollapseEvents);

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

        void OnTick(ScriptState &state,
            Lock<Read<EventInput>, Write<Signals>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                Assertf(sp::starts_with(event.name, "/signal/"), "Event name should be /signal/<action>/<signal>");
                double eventValue = EventData::Visit(event.data, [](auto &data) {
                    using T = std::decay_t<decltype(data)>;

                    if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                        return (double)data;
                    } else if constexpr (std::is_convertible_v<T, bool>) {
                        return (double)(bool)data;
                    } else {
                        return 1.0;
                    }
                });

                auto eventName = std::string_view(event.name).substr("/signal/"s.size());
                auto delimiter = eventName.find('/');
                Assertf(delimiter != std::string_view::npos, "Event name should be /signal/<action>/<signal>");
                auto action = eventName.substr(0, delimiter);
                std::string signalName(eventName.substr(delimiter + 1));
                if (signalName.empty()) continue;

                SignalRef ref(ent, signalName);
                if (action == "toggle") {
                    auto &currentValue = ref.GetValue(lock);
                    if (glm::epsilonEqual(currentValue, eventValue, (double)std::numeric_limits<float>::epsilon())) {
                        ref.SetValue(lock, 0);
                    } else {
                        ref.SetValue(lock, eventValue);
                    }
                } else if (action == "set") {
                    ref.SetValue(lock, eventValue);
                } else if (action == "add") {
                    ref.SetValue(lock, ref.GetValue(lock) + eventValue);
                } else if (action == "clear") {
                    ref.ClearValue(lock);
                } else {
                    Errorf("Unknown signal action: '%s'", std::string(action));
                }
            }
        }
    };
    StructMetadata MetadataSignalFromEvent(typeid(SignalFromEvent),
        sizeof(SignalFromEvent),
        "SignalFromEvent",
        "",
        StructField::New("outputs", &SignalFromEvent::outputs));
    LogicScript<SignalFromEvent> signalFromEvent("signal_from_event", MetadataSignalFromEvent);

    struct EventFromSignal {
        robin_hood::unordered_map<std::string, SignalExpression> outputs;

        void OnTick(ScriptState &state, SendEventsLock lock, Entity ent, chrono_clock::duration interval) {
            for (auto &[name, expr] : outputs) {
                ZoneScopedN("EventFromSignal::sendOutputEvents");
                auto value = expr.Evaluate(lock);
                if (value >= 0.5) {
                    ZoneScopedN("EventFromSignal::SendEvent");
                    EventBindings::SendEvent(lock, ent, Event{name, ent, value});
                }
            }
        }
    };
    StructMetadata MetadataEventFromSignal(typeid(EventFromSignal),
        sizeof(EventFromSignal),
        "EventFromSignal",
        "",
        StructField::New(&EventFromSignal::outputs));
    LogicScript<EventFromSignal> eventFromSignal("event_from_signal", MetadataEventFromSignal);
    PhysicsScript<EventFromSignal> physicsEventFromSignal("physics_event_from_signal", MetadataEventFromSignal);

    struct ComponentFromEvent {
        std::vector<std::string> outputs;
        robin_hood::unordered_map<std::string, std::pair<const ComponentBase *, std::optional<StructField>>>
            componentCache;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(outputs.size());
            for (auto &fieldPath : outputs) {
                size_t delimiter = fieldPath.find('.');
                if (delimiter == std::string::npos) continue;
                auto componentName = fieldPath.substr(0, delimiter);
                auto fieldName = fieldPath.substr(delimiter + 1);
                if (componentName.empty() || fieldName.empty()) continue;

                state.definition.events.emplace_back("/set/" + fieldPath);
            }
            state.definition.filterOnEvent = true;
        }

        void OnTick(ScriptState &state,
            DynamicLock<Read<Name, EventInput>> lock,
            Entity ent,
            chrono_clock::duration interval) {
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

                const ecs::ComponentBase *comp;
                std::optional<StructField> field;
                auto existing = componentCache.find(std::string(fieldPath));
                if (existing != componentCache.end()) {
                    std::tie(comp, field) = existing->second;
                } else {
                    comp = LookupComponent(componentName);
                    if (!comp) {
                        Errorf("ComponentFromEvent unknown component: %s", componentName);
                        continue;
                    }
                    if (!comp->HasComponent(lock, ent)) continue;

                    field = ecs::GetStructField(comp->metadata.type, fieldPath);
                    if (!field) {
                        Errorf("ComponentFromEvent unknown component field: %s", fieldPath);
                        continue;
                    }

                    componentCache.emplace(fieldPath, std::pair{comp, field});
                }
                if (!comp->HasComponent(lock, ent)) continue;

                void *compPtr = comp->AccessMut(lock, ent);
                if (!compPtr) {
                    Errorf("ComponentFromEvent %s access returned null data: %s",
                        componentName,
                        ecs::ToString(lock, ent));
                    continue;
                }
                EventData::Visit(event.data, [&](auto &data) {
                    using T = std::decay_t<decltype(data)>;

                    if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                        ecs::WriteStructField(compPtr, *field, [&data](double &value) {
                            value = (double)data;
                        });
                    } else if constexpr (sp::is_glm_vec<T>()) {
                        if constexpr (T::length() == 2) {
                            ecs::WriteStructField(compPtr, *field, [&data](glm::dvec2 &value) {
                                value = (glm::dvec2)data;
                            });
                        } else if constexpr (T::length() == 3) {
                            ecs::WriteStructField(compPtr, *field, [&data](glm::dvec3 &value) {
                                value = (glm::dvec3)data;
                            });
                        } else if constexpr (T::length() == 4) {
                            ecs::WriteStructField(compPtr, *field, [&data](glm::dvec4 &value) {
                                value = (glm::dvec4)data;
                            });
                        } else {
                            Errorf("ComponentFromEvent '%s' incompatible vector type: setting %s to %s",
                                event.name,
                                typeid(T).name(),
                                comp->metadata.type.name());
                        }
                    } else {
                        Errorf("ComponentFromEvent '%s' incompatible type: setting %s to %s",
                            event.name,
                            typeid(T).name(),
                            comp->metadata.type.name());
                    }
                });
            }
        }
    };
    StructMetadata MetadataComponentFromEvent(typeid(ComponentFromEvent),
        sizeof(ComponentFromEvent),
        "ComponentFromEvent",
        "",
        StructField::New("outputs", &ComponentFromEvent::outputs));
    LogicScript<ComponentFromEvent> componentFromEvent("component_from_event", MetadataComponentFromEvent);
    PhysicsScript<ComponentFromEvent> physicsComponentFromEvent("physics_component_from_event",
        MetadataComponentFromEvent);
} // namespace sp::scripts
