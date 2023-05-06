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

    std::vector<SignalRef> SignalManager::GetSignals(const EntityRef &entity) {
        std::vector<SignalRef> results;
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (signal.entity == entity) {
                results.emplace_back(refPtr);
            }
        });
        return results;
    }

    void SignalManager::Tick(chrono_clock::duration maxTickInterval) {
        signalRefs.Tick(maxTickInterval, [this](std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (!refPtr) return;
            auto liveIndex = refPtr->liveIndex.exchange(std::numeric_limits<size_t>::max());
            auto stagingIndex = refPtr->stagingIndex.exchange(std::numeric_limits<size_t>::max());
            ecs::QueueStagingTransaction<Write<Signals>>([stagingIndex](auto &lock) {
                auto &signals = lock.Get<Signals>();
                signals.FreeSignal(stagingIndex);
            });
            ecs::QueueTransaction<Write<Signals>>([liveIndex](auto &lock) {
                auto &signals = lock.Get<Signals>();
                signals.FreeSignal(liveIndex);
            });
        });
    }
} // namespace ecs
