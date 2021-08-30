#include "NamedEntity.hh"

#include "ecs/EcsImpl.hh"

namespace ecs {
    const Tecs::Entity &NamedEntity::Get(Lock<Read<ecs::Name>> lock) {
        if (name.empty()) {
            ent = Tecs::Entity();
        } else {
            if (ent.Has<ecs::Name>(lock)) {
                if (ent.Get<ecs::Name>(lock) == name) return ent;
            }
            ent = EntityWith<ecs::Name>(lock, name);
        }
        return ent;
    }

    Tecs::Entity NamedEntity::Get(Lock<Read<ecs::Name>> lock) const {
        if (name.empty()) {
            return Tecs::Entity();
        } else {
            if (ent.Has<ecs::Name>(lock)) {
                if (ent.Get<ecs::Name>(lock) == name) return ent;
            }
            return EntityWith<ecs::Name>(lock, name);
        }
    }
} // namespace ecs
