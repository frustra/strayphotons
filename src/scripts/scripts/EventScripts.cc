/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EventQueue.hh"
#include "ecs/ScriptImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalStructAccess.hh"
#include "strayphotons/Hashing.hh"
#include "strayphotons/HeapString.hh"
#include "strayphotons/HeapVector.hh"

namespace sp::scripts {
    using namespace ecs;

    struct InitEvent {
        HeapVector<EventName> outputs;

        void OnLogicTick(ScriptState &state,
            DynamicLock<SendEventsLock> lock,
            Entity ent,
            chrono_clock::duration interval) {
            state.definition.logicTickFunc = {}; // Only execute once on first frame

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
    BuiltInScript<InitEvent> initEvent("init_event", MetadataInitEvent);

    struct EventGateBySignal {
        EventName inputEvent, outputEvent;
        SignalExpression gateExpression;

        void Init(ScriptState &state) {
            if (inputEvent.empty()) {
                state.definition.events.clear();
            } else {
                state.definition.events = {inputEvent};
            }
        }

        void OnEvent(ScriptState &state, Lock<SendEventsLock> lock, Entity ent, Event event) {
            if (outputEvent.empty()) return;

            if (gateExpression.EvaluateEvent(lock, event.data) >= 0.5) {
                event.name = outputEvent;
                EventBindings::SendEvent(lock, ent, event);
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
    BuiltInScript<EventGateBySignal> eventGateBySignal("event_gate_by_signal", MetadataEventGateBySignal);

    struct CollapseEvents {
        robin_hood::unordered_map<EventName, EventName, StringHash, StringEqual> mapping;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(mapping.size());
            for (auto &map : mapping) {
                state.definition.events.emplace_back(map.first);
            }
        }

        void OnTick(ScriptState &state, DynamicLock<SendEventsLock> lock, Entity ent, chrono_clock::duration interval) {
            if (state.eventQueue.Empty()) return;

            robin_hood::unordered_map<EventName, std::optional<Event>, StringHash, StringEqual> outputEvents;
            Event event;
            while (state.eventQueue.Poll(event)) {
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
    struct LogicCollapseEvents : public CollapseEvents {
        void OnLogicTick(ScriptState &state,
            DynamicLock<SendEventsLock> lock,
            Entity ent,
            chrono_clock::duration interval) {
            OnTick(state, lock, ent, interval);
        }
    };
    BuiltInScript<LogicCollapseEvents> collapseEvents("collapse_events", MetadataCollapseEvents);
    struct PhysicsCollapseEvents : public CollapseEvents {
        void OnPhysicsTick(ScriptState &state,
            DynamicLock<SendEventsLock> lock,
            Entity ent,
            chrono_clock::duration interval) {
            OnTick(state, lock, ent, interval);
        }
    };
    BuiltInScript<PhysicsCollapseEvents> physicsCollapseEvents("physics_collapse_events", MetadataCollapseEvents);

    struct SignalFromEvent {
        HeapVector<HeapString> outputs;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(outputs.size() * 4);
            for (auto &outputSignal : outputs) {
                state.definition.events.emplace_back("/signal/toggle/" + outputSignal);
                state.definition.events.emplace_back("/signal/set/" + outputSignal);
                state.definition.events.emplace_back("/signal/add/" + outputSignal);
                state.definition.events.emplace_back("/signal/clear/" + outputSignal);
            }
        }

        void ProcessEvent(Lock<Write<Signals>> lock, Entity ent, Event event) {
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
            if (signalName.empty()) return;

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
    };
    StructMetadata MetadataSignalFromEvent(typeid(SignalFromEvent),
        sizeof(SignalFromEvent),
        "SignalFromEvent",
        "",
        StructField::New("outputs", &SignalFromEvent::outputs));
    struct AsyncSignalFromEvent : public SignalFromEvent {
        void OnEvent(ScriptState &state, Lock<Write<Signals>> lock, Entity ent, Event event) {
            ProcessEvent(lock, ent, event);
        }
    };
    BuiltInScript<AsyncSignalFromEvent> asyncSignalFromEvent("async_signal_from_event", MetadataSignalFromEvent);
    struct LogicSignalFromEvent : public SignalFromEvent {
        void OnLogicTick(ScriptState &state, Lock<Write<Signals>> lock, Entity ent, chrono_clock::duration interval) {
            Event event;
            while (state.eventQueue.Poll(event)) {
                ProcessEvent(lock, ent, event);
            }
        }
    };
    BuiltInScript<LogicSignalFromEvent> logicSignalFromEvent("signal_from_event", MetadataSignalFromEvent);

    struct EventFromSignal {
        robin_hood::unordered_map<EventName, SignalExpression, StringHash, StringEqual> outputs;

        void OnTick(ScriptState &state, DynamicLock<SendEventsLock> lock, Entity ent, chrono_clock::duration interval) {
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
    struct LogicEventFromSignal : public EventFromSignal {
        void OnLogicTick(ScriptState &state,
            DynamicLock<SendEventsLock> lock,
            Entity ent,
            chrono_clock::duration interval) {
            OnTick(state, lock, ent, interval);
        }
    };
    BuiltInScript<LogicEventFromSignal> eventFromSignal("event_from_signal", MetadataEventFromSignal);
    struct PhysicsEventFromSignal : public EventFromSignal {
        void OnPhysicsTick(ScriptState &state,
            DynamicLock<SendEventsLock> lock,
            Entity ent,
            chrono_clock::duration interval) {
            OnTick(state, lock, ent, interval);
        }
    };
    BuiltInScript<PhysicsEventFromSignal> physicsEventFromSignal("physics_event_from_signal", MetadataEventFromSignal);

    struct ComponentFromEvent {
        HeapVector<HeapString> outputs;
        robin_hood::unordered_map<HeapString,
            std::pair<const ComponentBase *, std::optional<StructField>>,
            StringHash,
            StringEqual>
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

                const ecs::ComponentBase *comp = LookupComponent(componentName);
                if (!comp) {
                    Errorf("ComponentFromEvent unknown component: %s", componentName);
                    continue;
                }

                std::optional<StructField> field = ecs::GetStructField(comp->metadata.type, fieldPath);
                if (!field) {
                    Errorf("ComponentFromEvent unknown component field: %s", fieldPath);
                    continue;
                }

                componentCache.emplace(fieldPath, std::pair{comp, field});
                uint64_t componentPermission = 2 << (ecs::GetComponentIndex(*comp));
                state.definition.onEventFunc.readPermissions |= componentPermission;
                state.definition.onEventFunc.writePermissions |= componentPermission;

                state.definition.events.emplace_back("/set/" + fieldPath);
            }
        }

        void OnEvent(ScriptState &state, DynamicLock<Read<Name>> lock, Entity ent, Event event) {
            if (!sp::starts_with(event.name, "/set/")) {
                Errorf("Unexpected event received by component_from_event: %s", event.name);
                return;
            }

            auto fieldPath = std::string_view(event.name).substr("/set/"s.size());
            auto it = componentCache.find(fieldPath);
            if (it == componentCache.end()) return;
            const ecs::ComponentBase *comp = it->second.first;
            std::optional<StructField> field = it->second.second;
            if (!comp->HasComponent(lock, ent)) return;

            void *compPtr = comp->AccessMut(lock, ent);
            if (!compPtr) {
                Errorf("ComponentFromEvent %s access returned null data: %s", comp->name, ecs::ToString(lock, ent));
                return;
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
    };
    StructMetadata MetadataComponentFromEvent(typeid(ComponentFromEvent),
        sizeof(ComponentFromEvent),
        "ComponentFromEvent",
        "",
        StructField::New("outputs", &ComponentFromEvent::outputs));
    BuiltInScript<ComponentFromEvent> componentFromEvent("component_from_event", MetadataComponentFromEvent);
} // namespace sp::scripts
