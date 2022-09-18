#include "GameEntities.hh"

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

namespace sp::entities {

    const ecs::EntityRef Spawn = ecs::Name("global", "spawn");
    const ecs::EntityRef Player = ecs::Name("player", "player");
    const ecs::EntityRef Head = ecs::Name("player", "head");
    const ecs::EntityRef Flatview = ecs::Name("player", "flatview");
    const ecs::EntityRef VrHmd = ecs::Name("vr", "hmd");

} // namespace sp::entities
