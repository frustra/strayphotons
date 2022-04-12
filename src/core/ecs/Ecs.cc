#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    ECS World;

    std::string ToString(Lock<Read<Name>> lock, Entity e) {
        if (!e.Has<Name>(lock)) return std::to_string(e);
        auto ecsId = Tecs::IdentifierFromGeneration(e.generation);
        auto generation = Tecs::GenerationWithoutIdentifier(e.generation);
        return e.Get<Name>(lock).String() + "(" + (ecsId != World.GetInstanceId() ? "staging " : "") +
               std::to_string(generation) + ", " + std::to_string(e.index) + ")";
    }
} // namespace ecs
