#include "NamedEntity.hh"

#include "ecs/EcsImpl.hh"

namespace ecs {
    const Entity &NamedEntity::Get(Lock<Read<ecs::Name>> lock) {
        if (!ent.Has<ecs::Name>(lock) && name) ent = EntityWith<ecs::Name>(lock, name);
        return ent;
    }
} // namespace ecs
