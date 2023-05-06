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
        auto &signals = lock.Get<Signals>();
        auto &index = GetIndex(lock);
        size_t i = index.load();
        if (i < signals.signals.size()) {
            std::get<double>(signals.signals[i]) = value;
        } else {
            index = signals.NewSignal(value);
        }
    }

    void SignalRef::ClearValue(const Lock<Write<Signals>> &lock) const {
        Assertf(ptr, "SignalRef::ClearValue() called on null SignalRef");
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return; // Noop

        std::get<double>(signals[index]) = -std::numeric_limits<double>::infinity();
    }

    bool SignalRef::HasValue(const Lock<Read<Signals>> &lock) const {
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return false;

        return !std::isinf(std::get<double>(signals[index]));
    }

    const double &SignalRef::GetValue(const Lock<Read<Signals>> &lock) const {
        static const double empty = 0.0;
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return empty;

        return std::get<double>(signals[index]);
    }

    void SignalRef::SetBinding(const Lock<Write<Signals>> &lock, const SignalExpression &expr) const {
        Assertf(ptr, "SignalRef::SetBinding() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        auto &index = GetIndex(lock);
        size_t i = index.load();

        if (i < signals.signals.size()) {
            std::get<SignalExpression>(signals.signals[i]) = expr;
        } else {
            index = signals.NewSignal(expr);
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

        std::get<SignalExpression>(signals[index]) = SignalExpression();
    }

    bool SignalRef::HasBinding(const Lock<Read<Signals>> &lock) const {
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return false;

        return (bool)std::get<SignalExpression>(signals[index]);
    }

    const SignalExpression &SignalRef::GetBinding(const Lock<Read<Signals>> &lock) const {
        static const SignalExpression empty = {};
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return empty;

        return std::get<SignalExpression>(signals[index]);
    }

    double SignalRef::GetSignal(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        ZoneScoped;
        if (!ptr) return 0.0;
        auto &signals = lock.Get<Signals>().signals;
        size_t index = GetIndex(lock);
        if (index >= signals.size()) return 0.0;

        auto &[value, expression] = signals[index];
        if (!std::isinf(value)) return value;
        return expression.Evaluate(lock, depth);
    }

    bool SignalRef::operator==(const Entity &other) const {
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
