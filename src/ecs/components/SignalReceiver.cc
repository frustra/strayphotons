#include "ecs/components/SignalReceiver.hh"

#include "ecs/events/SignalChange.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SignalReceiver>::Load(SignalReceiver &receiver, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "amplifier") {
                receiver.SetAmplifier(param.second.get<double>());
            } else if (param.first == "offset") {
                receiver.SetOffset(param.second.get<double>());
            }
        }
        return true;
    }

    void SignalReceiver::SetSignal(Tecs::Entity signaller, float signal) {
        this->signallers[signaller] = signal;
    }

    void SignalReceiver::RemoveSignal(Tecs::Entity signaller) {
        this->signallers.erase(signaller);
    }

    float SignalReceiver::GetSignal() const {
        float signal = 0;
        for (auto &kv : this->signallers) {
            signal += kv.second;
        }

        return this->amplifier * signal + this->offset;
    }

    bool SignalReceiver::IsTriggered() const {
        return this->GetSignal() > (1.0f - SignalReceiver::TRIGGER_TOLERANCE);
    }

    void SignalReceiver::SetAmplifier(float amp) {
        this->amplifier = amp;
    }

    void SignalReceiver::SetOffset(float offs) {
        this->offset = offs;
    }

    const float SignalReceiver::TRIGGER_TOLERANCE = 10 * std::numeric_limits<float>::epsilon();
} // namespace ecs
