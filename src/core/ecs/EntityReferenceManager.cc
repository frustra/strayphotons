#include "EntityReferenceManager.hh"

#include "ecs/EcsImpl.hh"

#include <mutex>
#include <shared_mutex>

namespace ecs {
    EntityReferenceManager GEntityRefs;

    EntityRef EntityReferenceManager::Get(const Name &name) {
        EntityRef ref = nameRefs.Load(name);
        if (!ref) {
            std::lock_guard lock(mutex);
            ref = nameRefs.Load(name);
            if (ref) return ref;

            ref = make_shared<EntityRef::Ref>(name);
            nameRefs.Register(name, ref.ptr);
        }
        return ref;
    }

    EntityRef EntityReferenceManager::Get(const Entity &entity) {
        if (!entity) return EntityRef();

        std::shared_lock lock(mutex);
        if (IsLive(entity)) {
            if (liveRefs.count(entity) == 0) return EntityRef();
            return EntityRef(liveRefs[entity].lock());
        } else {
            if (stagingRefs.count(entity) == 0) return EntityRef();
            return EntityRef(stagingRefs[entity].lock());
        }
    }

    EntityRef EntityReferenceManager::Set(const Name &name, const Entity &entity) {
        Assertf(entity, "Trying to set EntityRef with null Entity");

        auto ref = Get(name);
        std::lock_guard lock(mutex);
        if (IsLive(entity)) {
            ref.ptr->liveEntity = entity;
            liveRefs[entity] = ref.ptr;
        } else {
            ref.ptr->stagingEntity = entity;
            stagingRefs[entity] = ref.ptr;
        }
        return ref;
    }

    void EntityReferenceManager::Tick(chrono_clock::duration maxTickInterval) {
        nameRefs.Tick(maxTickInterval, [this](std::shared_ptr<EntityRef::Ref> &refPtr) {
            EntityRef ref(refPtr);
            auto staging = ref.GetStaging();
            auto live = ref.GetLive();
            if (staging || live) {
                std::lock_guard lock(mutex);
                if (staging) stagingRefs.erase(staging);
                if (live) liveRefs.erase(live);
            }
        });
    }
} // namespace ecs
