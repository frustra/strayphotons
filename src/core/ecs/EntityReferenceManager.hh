#pragma once

#include "core/LockFreeMutex.hh"
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
        EntityRef Get(std::string_view scene, std::string_view entity);

        void Set(const Name &name, const Entity &ent);

    private:
        sp::LockFreeMutex mutex;
        robin_hood::unordered_map<Name, EntityRef> references;
    };

    extern EntityReferenceManager GEntityRefs;
} // namespace ecs
