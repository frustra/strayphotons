#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    ECS World;

    std::string ToString(Lock<Read<Name>> lock, Tecs::Entity e) {
        if (e.Exists(lock)) {
            if (e.Has<Name>(lock)) {
                return e.Get<Name>(lock) + "(" + std::to_string(e.id) + ")";
            } else {
                return "Entity(" + std::to_string(e.id) + ")";
            }
        } else {
            return "Entity(NULL)";
        }
    }
} // namespace ecs
