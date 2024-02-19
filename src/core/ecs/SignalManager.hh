/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/LockFreeMutex.hh"
#include "common/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/Signals.hh"

#include <limits>
#include <memory>

namespace ecs {
    class SignalManager {
    public:
        SignalManager() {}

        SignalRef GetRef(const SignalKey &signal);
        SignalRef GetRef(const EntityRef &entity, const std::string_view &signalName);
        SignalRef GetRef(const std::string_view &str, const EntityScope &scope = Name());
        void ClearEntity(const Lock<Write<Signals>> &lock, const EntityRef &entity);
        std::set<SignalRef> GetSignals(const std::string &search = "");
        std::set<SignalRef> GetSignals(const EntityRef &entity);

        void Tick(chrono_clock::duration maxTickInterval);

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingMap<SignalKey, SignalRef::Ref, 1000> signalRefs;
        std::vector<std::shared_ptr<SignalRef::Ref>> setValues;
    };

    struct SignalRef::Ref {
        SignalKey signal;
        size_t liveIndex = std::numeric_limits<size_t>::max();
        size_t stagingIndex = std::numeric_limits<size_t>::max();

        Ref(const SignalKey &signal) : signal(signal) {}
    };

    SignalManager *GetSignalManager(SignalManager *override = nullptr);
} // namespace ecs
