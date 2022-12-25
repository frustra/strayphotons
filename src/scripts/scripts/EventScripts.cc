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
} // namespace sp::scripts
