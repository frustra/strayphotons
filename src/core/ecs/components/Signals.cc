#include "Signals.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <optional>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    void Component<SignalOutput>::Apply(SignalOutput &dst, const SignalOutput &src, bool liveTarget) {
        for (auto &signal : src.signals) {
            // noop if key already exists
            dst.signals.emplace(signal.first, signal.second);
        }
    }

    template<>
    void Component<SignalBindings>::Apply(SignalBindings &dst, const SignalBindings &src, bool liveTarget) {
        for (auto &binding : src.bindings) {
            // noop if key already exists
            dst.bindings.emplace(binding.first, binding.second);
        }
    }

    void SignalOutput::SetSignal(const std::string &name, double value) {
        signals[name] = value;
    }

    void SignalOutput::ClearSignal(const std::string &name) {
        signals.erase(name);
    }

    bool SignalOutput::HasSignal(const std::string &name) const {
        return signals.count(name) > 0;
    }

    double SignalOutput::GetSignal(const std::string &name) const {
        auto signal = signals.find(name);
        if (signal != signals.end()) return signal->second;
        return 0.0;
    }

    void SignalBindings::SetBinding(const std::string &name, const std::string &expr, const Name &scope) {
        Tracef("SetBinding %s = %s", name, expr);
        bindings.emplace(name, SignalExpression{expr, scope});
    }

    void SignalBindings::SetBinding(const std::string &name, EntityRef entity, const std::string &signalName) {
        Tracef("SetBinding %s = %s/%s", name, entity.Name().String(), signalName);
        bindings.emplace(name, SignalExpression{entity.Name(), signalName});
    }

    void SignalBindings::ClearBinding(const std::string &name) {
        bindings.erase(name);
    }

    bool SignalBindings::HasBinding(const std::string &name) const {
        return bindings.count(name) > 0;
    }

    const SignalExpression &SignalBindings::GetBinding(const std::string &name) const {
        auto list = bindings.find(name);
        if (list != bindings.end()) {
            return list->second;
        }
        static const SignalExpression defaultExpr = {};
        return defaultExpr;
    }
} // namespace ecs
