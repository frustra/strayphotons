#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Name.hh"

#include <atomic>
#include <robin_hood.h>

namespace ecs {
    class EntityReferenceManager {
    public:
        EntityReferenceManager() {}

        EntityRef Get(const Name &name);
        EntityRef Get(const Entity &stagingEntity);

        void Set(const Name &name, const Entity &entity);
        void Set(const Entity &stagingEntity, const Entity &liveEntity);

        void Tick(chrono_clock::duration maxTickInterval);

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingMap<Name, EntityRef::Ref> nameRefs;
        sp::PreservingMap<Entity, EntityRef::Ref> stagingRefs;
    };

    struct EntityRef::Ref {
        ecs::Name name;
        std::atomic<Entity> stagingEntity;
        std::atomic<Entity> liveEntity;

        Ref(const ecs::Name &name) : name(name) {}
        Ref(const Entity &ent);
    };

    extern EntityReferenceManager GEntityRefs;
} // namespace ecs
