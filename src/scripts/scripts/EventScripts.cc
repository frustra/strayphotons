#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpression.hh"

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
} // namespace sp::scripts
