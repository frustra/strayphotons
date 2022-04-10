#pragma once

#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Name.hh"

#include <robin_hood.h>
#include <string_view>

namespace ecs {
    class EntityReferenceManager {
    public:
        EntityReferenceManager() {}

        EntityRef Get(const Name &name);
        EntityRef Get(const Entity &stagingEntity);

        void Set(const Name &name, const Entity &liveEntity);
        void Set(const Entity &stagingEntity, const Entity &liveEntity);

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingMap<Name, EntityRef::Ref> nameRefs;
        sp::PreservingMap<Entity, EntityRef::Ref> stagingRefs;
    };

    extern EntityReferenceManager GEntityRefs;
} // namespace ecs
