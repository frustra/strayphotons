#pragma once

#include "ecs/EntityRef.hh"

namespace sp::entities {

    extern const ecs::EntityRef Spawn;
    extern const ecs::EntityRef Player;
    extern const ecs::EntityRef Flatview;
    extern const ecs::EntityRef VrHmd;
    extern const ecs::EntityRef VrOrigin;

    const ecs::EntityRef &Head();

} // namespace sp::entities
