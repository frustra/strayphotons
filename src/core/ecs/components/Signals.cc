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

    void SignalOutput::SetSignal(const StringHandle &name, double value) {
        signals[name] = value;
    }

    void SignalOutput::ClearSignal(const StringHandle &name) {
        signals.erase(name);
    }

    bool SignalOutput::HasSignal(const StringHandle &name) const {
        return signals.count(name) > 0;
    }

    double SignalOutput::GetSignal(const StringHandle &name) const {
        auto signal = signals.find(name);
        if (signal != signals.end()) return signal->second;
        return 0.0;
    }

    void SignalBindings::SetBinding(const StringHandle &name, const std::string &expr, const Name &scope) {
        Tracef("SetBinding %s = %s", name, expr);
        bindings.emplace(name, SignalExpression{expr, scope});
    }

    void SignalBindings::SetBinding(const StringHandle &name, EntityRef entity, const StringHandle &signalName) {
        Tracef("SetBinding %s = %s/%s", name, entity.Name().String(), signalName);
        bindings.emplace(name, SignalExpression{entity.Name(), signalName});
    }

    void SignalBindings::ClearBinding(const StringHandle &name) {
        bindings.erase(name);
    }

    bool SignalBindings::HasBinding(const StringHandle &name) const {
        return bindings.count(name) > 0;
    }

    const SignalExpression &SignalBindings::GetBinding(const StringHandle &name) const {
        ZoneScoped;
        auto list = bindings.find(name);
        if (list != bindings.end()) {
            return list->second;
        }
        static const SignalExpression defaultExpr = {};
        return defaultExpr;
    }

    double SignalBindings::GetSignal(const DynamicLock<ReadSignalsLock> &lock,
        const Entity &ent,
        const StringHandle &name,
        size_t depth) {
        ZoneScoped;
        if (ent.Has<SignalOutput>(lock)) {
            auto &signalOutput = ent.Get<const SignalOutput>(lock);
            auto signal = signalOutput.signals.find(name);
            if (signal != signalOutput.signals.end()) return signal->second;
        }
        if (!ent.Has<SignalBindings>(lock)) return 0.0;

        auto &bindings = ent.Get<const SignalBindings>(lock).bindings;
        auto list = bindings.find(name);
        if (list == bindings.end()) return 0.0;
        return list->second.Evaluate(lock, depth);
    }
} // namespace ecs
