#pragma once

#include "Common.hh"

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <limits>
#include <map>

namespace ecs {
    class SignalReceiver {
    public:
        void SetSignal(Tecs::Entity signaller, float signal = 0);
        void RemoveSignal(Tecs::Entity signaller);
        float GetSignal() const;
        bool IsTriggered() const;
        void SetAmplifier(float amp);
        void SetOffset(float offs);

    private:
        float amplifier = 1.0f;
        float offset = 0.0f;
        std::map<Tecs::Entity, float> signallers;

        static const float TRIGGER_TOLERANCE;
    };

    static Component<SignalReceiver> ComponentSignalReceiver("signalReceiver"); // TODO: Rename this

    template<>
    bool Component<SignalReceiver>::Load(SignalReceiver &dst, const picojson::value &src);
} // namespace ecs
