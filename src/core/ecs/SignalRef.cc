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

    void SignalRef::AddSubscriber(const Lock<Write<Signals>> &lock, const SignalRef &subscriber) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::AddSubscriber() called with staging lock");
        Assertf(ptr, "SignalRef::AddSubscriber() called on null SignalRef");
        Assertf(subscriber, "SignalRef::AddSubscriber() called with null subscriber");
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();
        if (index < signals.signals.size()) {
            auto &signal = signals.signals[index];
            sp::erase_if(signal.subscribers, [](auto &weakPtr) {
                return weakPtr.expired();
            });
            if (!sp::contains(signal.subscribers, subscriber.ptr)) {
                signal.subscribers.emplace_back(subscriber.ptr);
                signals.MarkStorageDirty(lock, index);
            }
        } else {
            index = signals.NewSignal(lock, *this, subscriber);
        }
    }

    void SignalRef::MarkDirty(const Lock<Write<Signals>> &lock, size_t depth) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::MarkDirty() called with staging lock");
        Assertf(ptr, "SignalRef::MarkDirty() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();
        if (index >= signals.signals.size()) return;
        auto &signal = signals.signals[index];
        //! signal.lastValueDirty &&
        if (depth <= MAX_SIGNAL_BINDING_DEPTH) {
            signal.lastValueDirty = true;
            signals.MarkStorageDirty(lock, index);
            if (depth >= MAX_SIGNAL_BINDING_DEPTH) {
                // Subscribers past this depth won't be able to evaluate this reference
                return;
            }
            for (const auto &sub : signal.subscribers) {
                auto subscriber = sub.lock();
                if (subscriber) {
                    SignalRef(subscriber).MarkDirty(lock, depth + 1);
                }
            }
        }
    }

    void SignalRef::UpdateDirtySubscribers(const Lock<Write<Signals>, ReadSignalsLock> &lock, size_t depth) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::MarkDirty() called with staging lock");
        Assertf(ptr, "SignalRef::MarkDirty() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();
        if (index >= signals.signals.size()) return;
        auto &signal = signals.signals[index];
        if (signal.lastValueDirty) {
            // double oldValue = signal.lastValue;
            signal.lastValue = signal.Value(lock);
            // if (signal.lastValue != oldValue) {
            //     Logf("Signal changed: %s = %f -> %f", signal.ref.String(), oldValue, signal.lastValue);
            // }
            signal.lastValueDirty = false;
            signals.MarkStorageDirty(lock, index);
            if (depth >= MAX_SIGNAL_BINDING_DEPTH) {
                // Subscribers past this depth won't be able to evaluate this reference
                return;
            }
            for (const auto &subscriber : signal.subscribers) {
                SignalRef(subscriber.lock()).UpdateDirtySubscribers(lock, depth + 1);
            }
        }
    }

    double &SignalRef::SetValue(const Lock<Write<Signals>> &lock, double value) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::SetValue() called with staging lock. Use SignalOutput instead");
        Assertf(ptr, "SignalRef::SetValue() called on null SignalRef");
        Assertf(std::isfinite(value), "SignalRef::SetValue() called with non-finite value: %f", value);
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();
        if (index < signals.signals.size()) {
            auto &signal = signals.signals[index];
            if (signal.value != value) {
                signal.value = value;
                signals.MarkStorageDirty(lock, index);
            }
            if (signal.lastValue != value) {
                // if (signal.ref.GetEntity().Name().scene != "input") {
                //     Logf("Changing signal: %s = %f -> %f", signal.ref.String(), signal.lastValue, value);
                // }
                signal.lastValue = value;
                MarkDirty(lock);
            }
            signal.lastValueDirty = false;
            return signal.value;
        } else {
            index = signals.NewSignal(lock, *this, value);
            if (value != 0.0) {
                // auto &signal = signals.signals[index];
                // if (signal.ref.GetEntity().Name().scene != "input" &&
                //     !sp::starts_with(signal.ref.GetSignalName(), "tile.")) {
                //     Logf("New signal: %s = %f", signal.ref.String(), value);
                // }
                MarkDirty(lock);
            }
            auto &signal = signals.signals[index];
            signal.lastValueDirty = false;
            return signal.value;
        }
    }

    void SignalRef::ClearValue(const Lock<Write<Signals>> &lock) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::ClearValue() called with staging lock. Use SignalOutput instead");
        Assertf(ptr, "SignalRef::ClearValue() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        const size_t &index = GetIndex();
        if (index >= signals.signals.size()) return; // Noop

        auto &signal = signals.signals[index];
        if (!std::isinf(signal.value)) {
            signal.value = -std::numeric_limits<double>::infinity();
            signals.MarkStorageDirty(lock, index);
        }
        if (signal.expr) {
            MarkDirty(lock);
        } else if (signal.lastValue != 0.0 || signal.lastValueDirty) {
            signal.lastValue = 0.0;
            MarkDirty(lock);
            signal.lastValueDirty = false;
        }
        if (!signal.expr && signal.subscribers.empty()) signals.FreeSignal(lock, index);
    }

    bool SignalRef::HasValue(const Lock<Read<Signals>> &lock) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::HasValue() called with staging lock. Use SignalOutput instead");
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return false;

        return !std::isinf(signals[index].value);
    }

    const double &SignalRef::GetValue(const Lock<Read<Signals>> &lock) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::GetValue() called with staging lock. Use SignalOutput instead");
        static const double empty = 0.0;
        if (!ptr) return empty;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return empty;

        return signals[index].value;
    }

    SignalExpression &SignalRef::SetBinding(const Lock<Write<Signals>, ReadSignalsLock> &lock,
        const SignalExpression &expr) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::SetBinding() called with staging lock. Use SignalBindings instead");
        Assertf(ptr, "SignalRef::SetBinding() called on null SignalRef");
        Assertf(expr, "SignalRef::SetBinding() called with null SignalExpression");
        auto &signals = lock.Get<Signals>();
        size_t &index = GetIndex();

        if (index < signals.signals.size()) {
            auto &signal = signals.signals[index];
            if (signal.expr != expr) {
                signal.expr = expr;
                signal.expr.rootNode->SubscribeToChildren(lock, *this);
                signals.MarkStorageDirty(lock, index);
                MarkDirty(lock);
            }
            // SubscribeToChildren may create new signal indexes, so we can't hold this reference earlier
            return signals.signals[index].expr;
        } else {
            index = signals.NewSignal(lock, *this, expr);
            signals.signals[index].expr.rootNode->SubscribeToChildren(lock, *this);
            MarkDirty(lock);
            // SubscribeToChildren may create new signal indexes, so we can't hold this reference earlier
            return signals.signals[index].expr;
        }
    }

    SignalExpression &SignalRef::SetBinding(const Lock<Write<Signals>, ReadSignalsLock> &lock,
        const std::string_view &expr,
        const EntityScope &scope) const {
        return SetBinding(lock, SignalExpression{expr, scope});
    }

    void SignalRef::ClearBinding(const Lock<Write<Signals>> &lock) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::ClearBinding() called with staging lock. Use SignalBindings instead");
        Assertf(ptr, "SignalRef::ClearBinding() called on null SignalRef");
        auto &signals = lock.Get<Signals>();
        const size_t &index = GetIndex();
        if (index >= signals.signals.size()) return; // Noop

        auto &signal = signals.signals[index];
        if (signal.expr) {
            signal.expr = SignalExpression();
            signals.MarkStorageDirty(lock, index);
            MarkDirty(lock);
        }
        if (std::isinf(signal.value) && signal.subscribers.empty()) signals.FreeSignal(lock, index);
    }

    bool SignalRef::HasBinding(const Lock<Read<Signals>> &lock) const {
        ZoneScoped;
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::HasBinding() called with staging lock. Use SignalBindings instead");
        if (!ptr) return false;
        auto &signals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= signals.size()) return false;

        return !signals[index].expr.IsNull();
    }

    const SignalExpression &SignalRef::GetBinding(const Lock<Read<Signals>> &lock) const {
        ZoneScoped;
        // ZoneStr(String());
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
        // ZoneStr(String());
        Assertf(IsLive(lock), "SiganlRef::GetSignal() called with staging lock. Use SignalBindings instead");
        if (!ptr) return 0.0;
        auto &readSignals = lock.Get<Signals>().signals;
        const size_t &index = GetIndex();
        if (index >= readSignals.size()) return 0.0;

        auto &readSignal = readSignals[index];
        bool hasValue = !std::isinf(readSignal.value);
        bool isUncacheableExpr = !hasValue && !readSignal.expr.IsCacheable();
        if (!isUncacheableExpr && !readSignal.lastValueDirty) {
            DebugAssertf(std::isfinite(readSignal.lastValue),
                "SignalRef::GetSignal() returned non-finite value: %f",
                readSignal.lastValue);
            return readSignal.lastValue;
        }
        auto writeLock = lock.TryLock<Write<Signals>>();
        if (writeLock) {
            auto &signals = writeLock->Get<Signals>();
            auto &signal = signals.signals[index];
            double newValue = signal.Value(lock, depth);
            if (isUncacheableExpr) return newValue;
            // if (newValue != signal.lastValue) {
            //     Logf("Signal changed (write eval): %s = %f -> %f", signal.ref.String(), signal.lastValue, newValue);
            // }
            signal.lastValue = newValue;
            signals.MarkStorageDirty(*writeLock, index);
            signal.lastValueDirty = false;
            return newValue;
        } else {
            return readSignal.Value(lock, depth);
        }
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
