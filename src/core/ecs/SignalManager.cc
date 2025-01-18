/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalManager.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/SignalRef.hh"

#include <mutex>
#include <shared_mutex>

namespace ecs {
    SignalManager &GetSignalManager() {
        static SignalManager signalManager;
        return signalManager;
    }

    SignalRef SignalManager::GetRef(const SignalKey &signal) {
        if (!signal) return SignalRef();

        SignalRef ref = signalRefs.Load(signal);
        if (!ref) {
            std::lock_guard lock(mutex);
            ref = signalRefs.Load(signal);
            if (ref) return ref;

            ref = make_shared<SignalRef::Ref>(signal);
            signalRefs.Register(signal, ref.ptr);
        }
        return ref;
    }

    SignalRef SignalManager::GetRef(const EntityRef &entity, const std::string_view &signalName) {
        return GetRef(SignalKey{entity, signalName});
    }

    SignalRef SignalManager::GetRef(const std::string_view &str, const EntityScope &scope) {
        return GetRef(SignalKey{str, scope});
    }

    std::set<SignalRef> SignalManager::GetSignals(const std::string &search) {
        std::set<SignalRef> results;
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (search.empty() || signal.String().find(search) != std::string::npos) {
                results.emplace(refPtr);
            }
        });
        return results;
    }

    std::set<SignalRef> SignalManager::GetSignals(const EntityRef &entity) {
        std::set<SignalRef> results;
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (signal.entity == entity) {
                results.emplace(refPtr);
            }
        });
        return results;
    }

    void SignalManager::Tick(chrono_clock::duration maxTickInterval) {
        std::vector<std::shared_ptr<SignalRef::Ref>> refsToFree;
        signalRefs.Tick(maxTickInterval, [&](std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (!refPtr) return;
            refsToFree.emplace_back(refPtr);
        });
        if (!refsToFree.empty()) {
            ZoneScopedN("FreeSignals");
            auto lock = ecs::StartTransaction<Write<Signals>>();
            auto &signals = lock.Get<Signals>();
            for (auto &refPtr : refsToFree) {
                signals.FreeSignal(lock, refPtr->index);
                refPtr->index = std::numeric_limits<size_t>::max();
            }
        }
    }
} // namespace ecs
