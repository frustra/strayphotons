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

    void SignalManager::ClearEntity(const Lock<Write<Signals>> &lock, const EntityRef &entity) {
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (refPtr && signal.entity == entity) {
                size_t index;
                if (IsLive(lock)) {
                    index = refPtr->liveIndex.exchange(std::numeric_limits<size_t>::max());
                } else if (IsStaging(lock)) {
                    index = refPtr->stagingIndex.exchange(std::numeric_limits<size_t>::max());
                } else {
                    Abortf("Invalid SignalManager::ClearEntity lock: %u", lock.GetInstance().GetInstanceId());
                }
                auto &signals = lock.Get<Signals>();
                signals.FreeSignal(index);
            }
        });
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
        ZoneScoped;
        setValues.clear();
        auto stagingLock = ecs::StagingWorld().StartTransaction<Write<Signals>>();
        auto liveLock = ecs::World().StartTransaction<Write<Signals>>();
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (!signal || !refPtr) return;
            SignalRef ref(refPtr);
            if (ref.HasValue(liveLock) || ref.HasBinding(liveLock) || ref.HasValue(stagingLock) ||
                ref.HasBinding(stagingLock)) {
                setValues.emplace_back(refPtr);
            }
        });
        signalRefs.Tick(maxTickInterval, [&](std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (!refPtr) return;
            auto stagingIndex = refPtr->stagingIndex.exchange(std::numeric_limits<size_t>::max());
            auto liveIndex = refPtr->liveIndex.exchange(std::numeric_limits<size_t>::max());
            stagingLock.Get<Signals>().FreeSignal(stagingIndex);
            liveLock.Get<Signals>().FreeSignal(liveIndex);
        });
    }
} // namespace ecs
