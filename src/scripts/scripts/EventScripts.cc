#include "core/Common.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    std::array eventScripts = {
        InternalScript("event_gate_by_signal",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                auto signalName = state.GetParam<string>("signal_name");
                auto signalValue = SignalBindings::GetSignal(lock, ent, signalName);

                double signalThreshold = 0.5;
                if (state.HasParam<double>("signal_threshold")) {
                    signalThreshold = state.GetParam<double>("signal_threshold");
                }

                bool gateOpen = signalValue >= signalThreshold;

                auto inputEvent = state.GetParam<string>("input_event");
                auto outputEvent = state.GetParam<string>("output_event");

                Event event;
                while (EventInput::Poll(lock, ent, inputEvent, event)) {
                    if (!gateOpen) continue;

                    EventBindings::SendEvent(lock, outputEvent, ent, event.data);
                }
            }),
    };
} // namespace ecs
