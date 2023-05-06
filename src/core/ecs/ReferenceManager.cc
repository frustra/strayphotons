#include "ReferenceManager.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/SignalRef.hh"

#include <mutex>
#include <shared_mutex>

namespace ecs {
    ReferenceManager &GetRefManager() {
        static ReferenceManager referenceManager;
        return referenceManager;
    }

    EntityRef ReferenceManager::GetEntity(const Name &name) {
        if (!name) return EntityRef();

        EntityRef ref = entityRefs.Load(name);
        if (!ref) {
            std::lock_guard lock(entityMutex);
            ref = entityRefs.Load(name);
            if (ref) return ref;

            ref = make_shared<EntityRef::Ref>(name);
            entityRefs.Register(name, ref.ptr);
        }
        return ref;
    }

    EntityRef ReferenceManager::GetEntity(const Entity &entity) {
        if (!entity) return EntityRef();

        std::shared_lock lock(entityMutex);
        if (IsLive(entity)) {
            if (liveRefs.count(entity) == 0) return EntityRef();
            return EntityRef(liveRefs[entity].lock());
        } else if (IsStaging(entity)) {
            if (stagingRefs.count(entity) == 0) return EntityRef();
            return EntityRef(stagingRefs[entity].lock());
        } else {
            Abortf("Invalid ReferenceManager entity: %s", std::to_string(entity));
        }
    }

    EntityRef ReferenceManager::SetEntity(const Name &name, const Entity &entity) {
        Assertf(entity, "Trying to set EntityRef with null Entity");

        auto ref = GetEntity(name);
        std::lock_guard lock(entityMutex);
        if (IsLive(entity)) {
            ref.ptr->liveEntity = entity;
            liveRefs[entity] = ref.ptr;
        } else if (IsStaging(entity)) {
            ref.ptr->stagingEntity = entity;
            stagingRefs[entity] = ref.ptr;
        } else {
            Abortf("Invalid ReferenceManager entity: %s", std::to_string(entity));
        }
        return ref;
    }

    std::set<Name> ReferenceManager::GetEntityNames(const std::string &search) {
        std::set<Name> results;
        entityRefs.ForEach([&](auto &name, auto &) {
            if (search.empty() || name.String().find(search) != std::string::npos) {
                results.emplace(name);
            }
        });
        return results;
    }

    SignalRef ReferenceManager::GetSignal(const SignalKey &signal) {
        if (!signal) return SignalRef();

        SignalRef ref = signalRefs.Load(signal);
        if (!ref) {
            std::lock_guard lock(signalMutex);
            ref = signalRefs.Load(signal);
            if (ref) return ref;

            ref = make_shared<SignalKey>(signal);
            signalRefs.Register(signal, ref.ptr);
        }
        return ref;
    }

    std::set<SignalKey> ReferenceManager::GetSignals(const std::string &search) {
        std::set<SignalKey> results;
        signalRefs.ForEach([&](auto &signal, auto &) {
            if (search.empty() || signal.String().find(search) != std::string::npos) {
                results.emplace(signal);
            }
        });
        return results;
    }

    void ReferenceManager::Tick(chrono_clock::duration maxTickInterval) {
        entityRefs.Tick(maxTickInterval, [this](std::shared_ptr<EntityRef::Ref> &refPtr) {
            EntityRef ref(refPtr);
            auto staging = ref.GetStaging();
            auto live = ref.GetLive();
            if (staging || live) {
                std::lock_guard lock(entityMutex);
                if (staging) stagingRefs.erase(staging);
                if (live) liveRefs.erase(live);
            }
        });
    }
} // namespace ecs
