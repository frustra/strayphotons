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

#include <optional>
#include <picojson/picojson.h>

namespace ecs {
    SignalKey::SignalKey(const EntityRef &entity, const std::string_view &signalName)
        : entity(entity), signalName(signalName) {
        Assertf(signalName.find_first_of(",():/# ") == std::string::npos,
            "Signal name has invalid character: '%s'",
            signalName);
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
    size_t Signals::NewSignal(const Lock<> &lock, const SignalRef &ref, double value) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(value, ref);
        } else {
            index = freeIndexes.top();
            freeIndexes.pop();
            signals[index] = Signal(value, ref);
        }
        Entity ent = ref.GetEntity().Get(lock);
        if (!ent.Exists(lock)) {
            Warnf("Setting signal value on missing entity");
            entityMapping.emplace(Entity(), index);
        } else {
            entityMapping.emplace(ent, index);
        }
        return index;
    }

    size_t Signals::NewSignal(const Lock<> &lock, const SignalRef &ref, const SignalExpression &expr) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(expr, ref);
        } else {
            index = freeIndexes.top();
            freeIndexes.pop();
            signals[index] = Signal(expr, ref);
        }
        Entity ent = ref.GetEntity().Get(lock);
        if (!ent.Exists(lock)) {
            Warnf("Setting signal expression on missing entity");
            entityMapping.emplace(Entity(), index);
        } else {
            entityMapping.emplace(ent, index);
        }
        return index;
    }

    void Signals::FreeSignal(const Lock<> &lock, size_t index) {
        if (index >= signals.size()) return;
        Signal &signal = signals[index];
        auto entity = signal.ref.GetEntity().Get(lock);
        auto range = entityMapping.equal_range(entity);
        for (auto it = range.first; it != range.second; it++) {
            if (it->second == index) {
                entityMapping.erase(it);
                break;
            }
        }
        if (signal.ref) signal.ref.GetIndex(lock) = std::numeric_limits<size_t>::max();
        signal = Signal();
        freeIndexes.push(index);
    }

    void Signals::FreeEntitySignals(const Lock<> &lock, Entity entity) {
        auto range = entityMapping.equal_range(entity);
        for (auto it = range.first; it != range.second; it++) {
            Signal &signal = signals[it->second];
            signal.ref.GetIndex(lock) = std::numeric_limits<size_t>::max();
            signal = Signal();
            freeIndexes.push(it->second);
        }
        entityMapping.erase(entity);
    }

    void Signals::PopulateMissingEntityRefs(const Lock<> &lock, Entity entity) {
        Assertf(entity.Exists(lock), "Signals::PopulateMissingEntityRefs called with missing entity");
        auto range = entityMapping.equal_range(Entity());
        sp::InlineVector<size_t, 1000> newEntityMappings;
        for (auto it = range.first; it != range.second;) {
            Signal &signal = signals[it->second];
            if (signal.ref.GetEntity() == entity) {
                newEntityMappings.emplace_back(it->second);
                it = entityMapping.erase(it);
            } else {
                it++;
            }
        }
        for (size_t index : newEntityMappings) {
            entityMapping.emplace(entity, index);
        }
    }

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
} // namespace ecs
