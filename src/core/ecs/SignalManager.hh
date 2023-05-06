#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/Signals.hh"

#include <atomic>
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
        std::vector<SignalRef> GetSignals(const EntityRef &entity);

        void Tick(chrono_clock::duration maxTickInterval);

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingMap<SignalKey, SignalRef::Ref, 1000> signalRefs;
    };

    struct SignalRef::Ref {
        SignalKey signal;
        std::atomic_size_t liveIndex;
        std::atomic_size_t stagingIndex;

        Ref(const SignalKey &signal)
            : signal(signal), liveIndex(std::numeric_limits<size_t>::max()),
              stagingIndex(std::numeric_limits<size_t>::max()) {}
    };

    SignalManager &GetSignalManager();
} // namespace ecs
