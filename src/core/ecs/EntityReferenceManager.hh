#pragma once

#include "core/Common.hh"
#include "core/EntityMap.hh"
#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Name.hh"

#include <atomic>
#include <memory>
#include <robin_hood.h>

namespace ecs {
    class EntityReferenceManager {
    public:
        EntityReferenceManager() {}

        EntityRef Get(const Name &name);
        EntityRef Get(const Entity &entity);
        EntityRef Set(const Name &name, const Entity &entity);
        std::set<Name> GetNames(const std::string &search = "");

        void Tick(chrono_clock::duration maxTickInterval);

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingMap<Name, EntityRef::Ref> nameRefs;
        sp::EntityMap<std::weak_ptr<EntityRef::Ref>> stagingRefs;
        sp::EntityMap<std::weak_ptr<EntityRef::Ref>> liveRefs;
    };

    struct EntityRef::Ref {
        ecs::Name name;
        std::atomic<Entity> stagingEntity;
        std::atomic<Entity> liveEntity;

        Ref(const ecs::Name &name) : name(name) {}
        Ref(const Entity &ent);
    };

    EntityReferenceManager &GetEntityRefs();
} // namespace ecs
