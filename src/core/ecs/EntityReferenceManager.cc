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

    EntityRef EntityReferenceManager::Get(const Entity &stagingEntity) {
        EntityRef ref = stagingRefs.Load(stagingEntity);
        if (!ref) {
            std::lock_guard lock(mutex);
            ref = stagingRefs.Load(stagingEntity);
            if (ref) return ref;

            ref = make_shared<EntityRef::Ref>(stagingEntity);
            stagingRefs.Register(stagingEntity, ref.ptr);
        }
        return ref;
    }

    void EntityReferenceManager::Set(const Name &name, const Entity &entity) {
        return Get(name).Set(entity);
    }

    void EntityReferenceManager::Set(const Entity &stagingEntity, const Entity &liveEntity) {
        return Get(stagingEntity).Set(liveEntity);
    }

    void EntityReferenceManager::Tick(chrono_clock::duration maxTickInterval) {
        nameRefs.Tick(maxTickInterval);
        stagingRefs.Tick(maxTickInterval);
    }
} // namespace ecs
