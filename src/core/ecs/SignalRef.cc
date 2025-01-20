/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/SignalManager.hh"

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

    size_t &SignalRef::GetIndex() const {
        Assert(ptr, "SignalRef::GetIndex() called on null SignalRef");
        return ptr->index;
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

    void SignalRef::SetScope(const EntityScope &scope) {
        if (!ptr) return;
        ecs::EntityRef newRef = ptr->signal.entity;
        newRef.SetScope(scope);
        if (!newRef) {
            ptr = nullptr;
        } else if (newRef != ptr->signal.entity) {
            ptr = GetSignalManager().GetRef(newRef, ptr->signal.signalName).ptr;
        }
    }

    double &SignalRef::SetValue(const Lock<Write<Signals>> &lock, double value) const {
        Assertf(IsLive(lock), "SiganlRef::SetValue() called with staging lock. Use SignalOutput instead");
        Assertf(ptr, "SignalRef::SetValue() called on null SignalRef");
        Assertf(std::isfinite(value), "SignalRef::SetValue() called with non-finite value: %f", value);
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();
        if (index < signals.signals.size()) {
            auto &signal = signals.signals[index];
            signal.value = value;
            signals.MarkDirty(lock, index);
            return signal.value;
        } else {
            index = signals.NewSignal(lock, *this, value);
            signals.MarkDirty(lock, index);
            return signals.signals[index].value;
        }
    }

    void SignalRef::ClearValue(const Lock<Write<Signals>> &lock) const {
        Assertf(IsLive(lock), "SiganlRef::ClearValue() called with staging lock. Use SignalOutput instead");
        Assertf(ptr, "SignalRef::ClearValue() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        const size_t &index = GetIndex();
        if (index >= signals.signals.size()) return; // Noop

        auto &signal = signals.signals[index];
        signal.value = -std::numeric_limits<double>::infinity();
        signals.MarkDirty(lock, index);
        if (signal.expr.IsNull()) signals.FreeSignal(lock, index);
    }

    bool SignalRef::HasValue(const Lock<Read<Signals>> &lock) const {
        Assertf(IsLive(lock), "SiganlRef::HasValue() called with staging lock. Use SignalOutput instead");
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return false;

        return !std::isinf(signals[index].value);
    }

    const double &SignalRef::GetValue(const Lock<Read<Signals>> &lock) const {
        Assertf(IsLive(lock), "SiganlRef::GetValue() called with staging lock. Use SignalOutput instead");
        static const double empty = 0.0;
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return empty;

        return signals[index].value;
    }

    SignalExpression &SignalRef::SetBinding(const Lock<Write<Signals>> &lock, const SignalExpression &expr) const {
        Assertf(IsLive(lock), "SiganlRef::SetBinding() called with staging lock. Use SignalBindings instead");
        Assertf(ptr, "SignalRef::SetBinding() called on null SignalRef");
        Assertf(!expr.IsNull(), "SignalRef::SetBinding() called with null SignalExpression");
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();

        if (index < signals.signals.size()) {
            auto &signal = signals.signals[index];
            signal.expr = expr;
            signals.MarkDirty(lock, index);
            return signal.expr;
        } else {
            index = signals.NewSignal(lock, *this, expr);
            signals.MarkDirty(lock, index);
            return signals.signals[index].expr;
        }
    }

    SignalExpression &SignalRef::SetBinding(const Lock<Write<Signals>> &lock,
        const std::string_view &expr,
        const EntityScope &scope) const {
        return SetBinding(lock, SignalExpression{expr, scope});
    }

    void SignalRef::ClearBinding(const Lock<Write<Signals>> &lock) const {
        Assertf(IsLive(lock), "SiganlRef::ClearBinding() called with staging lock. Use SignalBindings instead");
        Assertf(ptr, "SignalRef::ClearBinding() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        const size_t &index = GetIndex();
        if (index >= signals.signals.size()) return; // Noop

        auto &signal = signals.signals[index];
        signal.expr = SignalExpression();
        signals.MarkDirty(lock, index);
        if (std::isinf(signal.value)) signals.FreeSignal(lock, index);
    }

    bool SignalRef::HasBinding(const Lock<Read<Signals>> &lock) const {
        Assertf(IsLive(lock), "SiganlRef::HasBinding() called with staging lock. Use SignalBindings instead");
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return false;

        return !signals[index].expr.IsNull();
    }

    const SignalExpression &SignalRef::GetBinding(const Lock<Read<Signals>> &lock) const {
        Assertf(IsLive(lock), "SiganlRef::GetBinding() called with staging lock. Use SignalBindings instead");
        static const SignalExpression empty = {};
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return empty;

        return signals[index].expr;
    }

    double SignalRef::GetSignal(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        ZoneScoped;
        Assertf(IsLive(lock), "SiganlRef::GetSignal() called with staging lock. Use SignalBindings instead");
        if (!ptr) return 0.0;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return 0.0;

        auto &signal = signals[index];
        if (!std::isinf(signal.value)) return signal.value;
        return signal.expr.Evaluate(lock, depth);
    }

    bool SignalRef::operator==(const Entity &other) const {
        if (!ptr || !other) return false;
        return ptr->signal.entity == other;
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
