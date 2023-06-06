/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Signals.hh"

#include "core/Common.hh"
#include "core/Hashing.hh"
#include "core/Logging.hh"
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
    size_t Signals::NewSignal(const SignalRef &ref, double value) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(value, ref);
        } else {
            auto it = freeIndexes.begin();
            index = *it;
            freeIndexes.erase(it);
            signals[index] = Signal(value, ref);
        }
        return index;
    }

    size_t Signals::NewSignal(const SignalRef &ref, const SignalExpression &expr) {
        size_t index;
        if (freeIndexes.empty()) {
            index = signals.size();
            signals.emplace_back(expr, ref);
        } else {
            auto it = freeIndexes.begin();
            index = *it;
            freeIndexes.erase(it);
            signals[index] = Signal(expr, ref);
        }
        return index;
    }

    void Signals::FreeSignal(size_t index) {
        if (index >= signals.size()) return;
        signals[index] = Signal();
        freeIndexes.insert(index);
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
