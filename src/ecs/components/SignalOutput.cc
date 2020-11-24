#include "SignalOutput.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SignalOutput>::Load(Lock<Read<ecs::Name>> lock, SignalOutput &output, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.second.is<bool>()) {
                output.SetSignal(param.first, param.second.get<bool>() ? 1.0 : 0.0);
            } else {
                output.SetSignal(param.first, param.second.get<double>());
            }
        }
        return true;
    }

    void SignalOutput::SetSignal(const std::string &name, double value) {
        signals[name] = value;
    }

    void SignalOutput::RemoveSignal(const std::string &name) {
        signals.erase(name);
    }

    const double &SignalOutput::GetSignal(const std::string &name) const {
        return signals.at(name);
    }

    const std::map<std::string, double> &SignalOutput::GetSignals() const {
        return signals;
    }
} // namespace ecs
