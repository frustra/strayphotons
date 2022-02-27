#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    ECS World;

    std::string ToString(Lock<Read<Name>> lock, Entity e) {
        if (!e.Has<Name>(lock)) return std::to_string(e);
        return e.Get<Name>(lock).String() + "(" + std::to_string(e.generation) + ", " + std::to_string(e.index) + ")";
    }

    std::string GetFullName(Lock<Read<Name>> lock, Entity e) {
        if (!e.Has<Name>(lock)) return "(" + std::to_string(e.generation) + "," + std::to_string(e.index) + ")";
        return e.Get<Name>(lock).String();
    }
} // namespace ecs
