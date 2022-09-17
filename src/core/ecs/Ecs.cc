#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    ECS &World() {
        static ECS world;
        return world;
    }

    ECS &StagingWorld() {
        static ECS stagingWorld;
        return stagingWorld;
    }

    std::string ToString(Lock<Read<Name>> lock, Entity e) {
        if (!e.Has<Name>(lock)) return std::to_string(e);
        auto generation = Tecs::GenerationWithoutIdentifier(e.generation);
        return e.Get<Name>(lock).String() + "(" + (IsLive(e) ? "gen " : "staging gen ") + std::to_string(generation) +
               ", index " + std::to_string(e.index) + ")";
    }
} // namespace ecs
