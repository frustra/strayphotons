/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Signals.hh"

#include "common/Common.hh"
#include "common/Hashing.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalManager.hh"

#include <optional>
#include <vector>

namespace ecs {
    SignalKey::SignalKey(const EntityRef &entity, const std::string_view &signalName)
        : entity(entity), signalName(signalName) {
        Assertf(signalName.find_first_of(",():/# ") == std::string::npos,
            "Signal name has invalid character: '%s'",
            std::string(signalName));
    }

    SignalKey::SignalKey(const std::string_view &str, const EntityScope &scope) {
        Parse(str, scope);
    }

    bool SignalKey::Parse(const std::string_view &str, const EntityScope &scope) {
        size_t i = str.find('/');
        if (i == std::string::npos) {
            entity = {};
            signalName.clear();
            Errorf("Invalid signal has no entity/signal separator: %s", std::string(str));
            return false;
        }
        ecs::Name entityName(str.substr(0, i), scope);
        if (!entityName) {
            entity = {};
            signalName.clear();
            Errorf("Invalid signal has bad entity name: %s", std::string(str));
            return false;
        }
        entity = entityName;
        signalName = str.substr(i + 1);
        return true;
    }

    std::ostream &operator<<(std::ostream &out, const SignalKey &v) {
        return out << v.String();
    }
} // namespace ecs

namespace std {
    std::size_t hash<ecs::SignalKey>::operator()(const ecs::SignalKey &key) const {
        auto val = hash<string>()(key.signalName);
        sp::hash_combine(val, key.entity.Name());
        return val;
    }
} // namespace std

namespace ecs {
    Signals::Signal::Signal() : value(-std::numeric_limits<double>::infinity()), expr(), lastValueDirty(true) {}
    Signals::Signal::Signal(const SignalRef &ref, double value) : value(value), expr(), ref(ref) {
        if (!std::isinf(value)) {
            this->lastValue = value;
            this->lastValueDirty = false;
        } else {
            this->lastValue = 0.0;
            this->lastValueDirty = true;
        }
    }
    Signals::Signal::Signal(const SignalRef &ref, const SignalExpression &expr)
        : value(-std::numeric_limits<double>::infinity()), expr(expr), ref(ref), lastValueDirty(true) {}
    Signals::Signal::Signal(const SignalRef &ref, const SignalRef &subscriber)
        : value(-std::numeric_limits<double>::infinity()), expr(), ref(ref), lastValueDirty(true) {
        subscribers.emplace_back(subscriber.GetWeakRef());
    }

    double Signals::Signal::Value(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        if (!std::isinf(value)) {
            return value;
        } else {
            return expr.Evaluate(lock, depth);
        }
    }

    size_t Signals::NewSignal(const Lock<Write<Signals>> &lock, const SignalRef &ref, double value) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(ref, value);
        } else {
            index = freeIndexes.top();
            freeIndexes.pop();
            signals[index] = Signal(ref, value);
        }
        MarkStorageDirty(lock, index);
        Entity ent = ref.GetEntity().Get(lock);
        Assertf(ent.Exists(lock), "Setting signal value on missing entity: %s", ref.GetEntity().Name().String());
        return index;
    }

    size_t Signals::NewSignal(const Lock<Write<Signals>, ReadSignalsLock> &lock,
        const SignalRef &ref,
        const SignalExpression &expr) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(ref, expr);
        } else {
            index = freeIndexes.top();
            freeIndexes.pop();
            signals[index] = Signal(ref, expr);
        }
        MarkStorageDirty(lock, index);
        Entity ent = ref.GetEntity().Get(lock);
        Assertf(ent.Exists(lock), "Setting signal expression on missing entity: %s", ref.GetEntity().Name().String());
        return index;
    }

    size_t Signals::NewSignal(const Lock<Write<Signals>> &lock, const SignalRef &ref, const SignalRef &subscriber) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(ref, subscriber);
        } else {
            index = freeIndexes.top();
            freeIndexes.pop();
            signals[index] = Signal(ref, subscriber);
        }
        MarkStorageDirty(lock, index);
        return index;
    }

    void Signals::FreeSignal(const Lock<Write<Signals>> &lock, size_t index) {
        if (index >= signals.size()) return;
        Signal &signal = signals[index];
        DebugAssertf(signal.subscribers.empty(), "Signals::FreeSignal index has subscribers");
        MarkStorageDirty(lock, index);
        if (signal.ref) {
            if (!std::isinf(signal.lastValue)) signal.ref.MarkDirty(lock);
            signal.ref.GetIndex() = std::numeric_limits<size_t>::max();
        }
        signal = Signal();
        freeIndexes.push(index);
    }

    void Signals::FreeEntitySignals(const Lock<Write<Signals>> &lock, Entity entity) {
        ZoneScoped;
        std::vector<size_t> updatedIndexes;
        for (size_t i = 0; i < signals.size(); i++) {
            Signal &signal = signals[i];
            if (signal.ref && signal.ref.GetEntity() == entity) {
                MarkStorageDirty(lock, i);
                signal.value = -std::numeric_limits<double>::infinity();
                signal.expr = SignalExpression();
                signal.ref.UnsubscribeDependencies(lock);
                if (signal.lastValue != 0.0 || signal.lastValueDirty) {
                    signal.lastValue = 0.0;
                    signal.ref.MarkDirty(lock);
                    signal.lastValueDirty = false;
                }
                updatedIndexes.emplace_back(i);
            }
        }
        for (size_t i : updatedIndexes) {
            Signal &signal = signals[i];
            if (signal.subscribers.empty()) {
                DebugAssertf(i == signal.ref.GetIndex(), "FreeEntitySignals index missmatch");
                signal.ref.GetIndex() = std::numeric_limits<size_t>::max();
                signal = Signal();
                freeIndexes.push(i);
            }
        }
    }

    void Signals::UpdateMissingEntitySignals(const Lock<Write<Signals>> &lock) {
        ZoneScoped;
        std::vector<size_t> updatedIndexes;
        for (size_t i = 0; i < signals.size(); i++) {
            Signal &signal = signals[i];
            if (signal.ref && !signal.ref.GetEntity().Get(lock).Exists(lock)) {
                MarkStorageDirty(lock, i);
                signal.value = -std::numeric_limits<double>::infinity();
                signal.expr = SignalExpression();
                signal.ref.UnsubscribeDependencies(lock);
                if (signal.lastValue != 0.0 || signal.lastValueDirty) {
                    signal.lastValue = 0.0;
                    signal.ref.MarkDirty(lock);
                    signal.lastValueDirty = false;
                }
                updatedIndexes.emplace_back(i);
            }
        }
        for (size_t i : updatedIndexes) {
            Signal &signal = signals[i];
            if (signal.subscribers.empty()) {
                DebugAssertf(i == signal.ref.GetIndex(), "UpdateMissingEntitySignals index missmatch");
                signal.ref.GetIndex() = std::numeric_limits<size_t>::max();
                signal = Signal();
                freeIndexes.push(i);
            }
        }
    }

    void Signals::MarkStorageDirty(const Lock<Write<Signals>> &lock, size_t index) {
        DebugZoneScoped;
        auto &signals = lock.Get<Signals>();
        auto &prevSignals = lock.GetPrevious<Signals>();
        if (signals.changeCount == prevSignals.changeCount) {
            signals.dirtyIndices.clear();
            signals.changeCount++;
        }
        Assertf(index < signals.signals.size(), "Signals::MarkStorageDirty index out of range");
        signals.dirtyIndices.emplace(index);
    }

    Signals &Signals::operator=(const Signals &other) {
        ZoneScoped;
        if (other.changeCount == changeCount) {
            // Noop
            DebugAssertf(signals.size() == other.signals.size() && dirtyIndices == other.dirtyIndices,
                "Changes are different");
        } else if (other.changeCount == changeCount + 1) {
            if (signals.size() != other.signals.size()) signals.resize(other.signals.size());
            dirtyIndices = other.dirtyIndices;
            for (size_t index : other.dirtyIndices) {
                signals[index] = other.signals[index];
            }
            freeIndexes = other.freeIndexes;
        } else {
            dirtyIndices = other.dirtyIndices;
            signals = other.signals;
            freeIndexes = other.freeIndexes;
        }
        changeCount = other.changeCount;
        return *this;
    }

    template<>
    void EntityComponent<SignalOutput>::Apply(SignalOutput &dst, const SignalOutput &src, bool liveTarget) {
        for (auto &signal : src.signals) {
            // noop if key already exists
            dst.signals.emplace(signal.first, signal.second);
        }
    }

    template<>
    void EntityComponent<SignalBindings>::Apply(SignalBindings &dst, const SignalBindings &src, bool liveTarget) {
        for (auto &binding : src.bindings) {
            // noop if key already exists
            dst.bindings.emplace(binding.first, binding.second);
        }
    }
} // namespace ecs
