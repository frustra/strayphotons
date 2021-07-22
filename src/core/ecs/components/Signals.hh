#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <limits>
#include <map>
#include <string>

namespace ecs {
    class SignalOutput {
    public:
        void SetSignal(const std::string &name, double value);
        void ClearSignal(const std::string &name);
        const double &GetSignal(const std::string &name) const;
        const std::map<std::string, double> &GetSignals() const;

    private:
        std::map<std::string, double> signals;
    };

    static Component<SignalOutput> ComponentSignalReceiver("signal_output");

    template<>
    bool Component<SignalOutput>::Load(Lock<Read<ecs::Name>> lock, SignalOutput &dst, const picojson::value &src);
} // namespace ecs
