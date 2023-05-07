#include "SignalRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/SignalManager.hh"

#include <atomic>
#include <limits>

namespace ecs {
    SignalRef::SignalRef(const EntityRef &ent, const std::string_view &signalName) {
        if (!ent || signalName.empty()) return;
        ptr = GetSignalManager().GetRef(ent, signalName).ptr;
    }

    SignalRef::SignalRef(const std::string_view &str, const EntityScope &scope) {
        if (str.empty()) return;
        ptr = GetSignalManager().GetRef(str, scope).ptr;
    }

    std::atomic_size_t &SignalRef::GetIndex(const Lock<> &lock) const {
        if (IsLive(lock)) {
            return GetLiveIndex();
        } else if (IsStaging(lock)) {
            return GetStagingIndex();
        } else {
            Abortf("Invalid SignalRef lock: %u", lock.GetInstance().GetInstanceId());
        }
    }

    std::atomic_size_t &SignalRef::GetLiveIndex() const {
        Assert(ptr, "SignalRef::GetLiveIndex() called on null SignalRef");
        return ptr->liveIndex;
    }

    std::atomic_size_t &SignalRef::GetStagingIndex() const {
        Assert(ptr, "SignalRef::GetStagingIndex() called on null SignalRef");
        return ptr->stagingIndex;
    }

    const EntityRef &SignalRef::GetEntity() const {
        static const EntityRef empty = {};
        return ptr ? ptr->signal.entity : empty;
    }

    const std::string &SignalRef::GetSignalName() const {
        static const std::string empty = "";
        return ptr ? ptr->signal.signalName : empty;
    }

    std::string SignalRef::String() const {
        if (!ptr) return "";
        return ptr->signal.String();
    }

    void SignalRef::SetValue(const Lock<Write<Signals>> &lock, double value) const {
        Assertf(ptr, "SignalRef::SetValue() called on null SignalRef");
        Assertf(std::isfinite(value), "SignalRef::SetValue() called with non-finite value: %f", value);
        auto &signals = lock.Get<Signals>();
        auto &index = GetIndex(lock);
        size_t i = index.load();
        if (i < signals.signals.size()) {
            auto &signal = signals.signals[i];
            signal.value = value;
            signal.ref = *this;
        } else {
            index = signals.NewSignal(*this, value);
        }
    }

    void SignalRef::ClearValue(const Lock<Write<Signals>> &lock) const {
        Assertf(ptr, "SignalRef::ClearValue() called on null SignalRef");
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return; // Noop

        auto &signal = signals[index];
        signal.value = -std::numeric_limits<double>::infinity();
        if (!signal.expr) signal.ref = {};
    }

    bool SignalRef::HasValue(const Lock<Read<Signals>> &lock) const {
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return false;

        return !std::isinf(signals[index].value);
    }

    const double &SignalRef::GetValue(const Lock<Read<Signals>> &lock) const {
        static const double empty = 0.0;
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return empty;

        return signals[index].value;
    }

    void SignalRef::SetBinding(const Lock<Write<Signals>> &lock, const SignalExpression &expr) const {
        Assertf(ptr, "SignalRef::SetBinding() called on null SignalRef");
        if (!expr) return ClearBinding(lock);
        auto &signals = lock.Get<Signals>();
        auto &index = GetIndex(lock);
        size_t i = index.load();

        if (i < signals.signals.size()) {
            auto &signal = signals.signals[i];
            signal.expr = expr;
            signal.ref = *this;
        } else {
            index = signals.NewSignal(*this, expr);
        }
    }

    void SignalRef::SetBinding(const Lock<Write<Signals>> &lock,
        const std::string_view &expr,
        const EntityScope &scope) const {
        SetBinding(lock, SignalExpression{expr, scope});
    }

    void SignalRef::ClearBinding(const Lock<Write<Signals>> &lock) const {
        Assertf(ptr, "SignalRef::ClearBinding() called on null SignalRef");
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return; // Noop

        auto &signal = signals[index];
        signal.expr = SignalExpression();
        if (std::isinf(signal.value)) signal.ref = {};
    }

    bool SignalRef::HasBinding(const Lock<Read<Signals>> &lock) const {
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return false;

        return (bool)signals[index].expr;
    }

    const SignalExpression &SignalRef::GetBinding(const Lock<Read<Signals>> &lock) const {
        static const SignalExpression empty = {};
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return empty;

        return signals[index].expr;
    }

    double SignalRef::GetSignal(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        ZoneNamedN(tracyCtx1, "SignalRef::GetSignal", true);
        if (!ptr) return 0.0;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return 0.0;

        ZoneNamedN(tracyCtx2, "SignalRef::GetSignalLookup", true);

        auto &signal = signals[index];
        if (!std::isinf(signal.value)) return signal.value;
        ZoneNamedN(tracyCtx3, "SignalRef::GetSignalEval", true);
        return signal.expr.Evaluate(lock, depth);
    }

    bool SignalRef::operator==(const EntityRef &other) const {
        if (!ptr || !other) return false;
        return ptr->signal.entity == other;
    }

    bool SignalRef::operator==(const std::string &other) const {
        if (!ptr || other.empty()) return false;
        return ptr->signal.signalName == other;
    }

    bool SignalRef::operator<(const SignalRef &other) const {
        if (!other.ptr) return false;
        if (!ptr) return true;
        return ptr->signal < other.ptr->signal;
    }
} // namespace ecs

namespace std {
    size_t hash<ecs::SignalRef>::operator()(const ecs::SignalRef &ref) const {
        return hash<size_t>()(reinterpret_cast<size_t>(static_cast<void *>(ref.ptr.get())));
    }
} // namespace std
