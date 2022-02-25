#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    ECS World;

    std::string ToString(Lock<Read<Name>> lock, Entity e) {
        if (e.Has<Name>(lock)) {
            return e.Get<Name>(lock) + "(" + std::to_string(e.generation) + ", " + std::to_string(e.index) + ")";
        } else {
            return std::to_string(e);
        }
    }
} // namespace ecs
