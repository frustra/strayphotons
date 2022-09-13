#include "GameEntities.hh"

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

namespace sp::entities {

    const ecs::EntityRef Spawn = ecs::Name("global", "spawn");
    const ecs::EntityRef Player = ecs::Name("player", "player");
    const ecs::EntityRef Flatview = ecs::Name("player", "flatview");
    const ecs::EntityRef VrHmd = ecs::Name("vr", "hmd");
    const ecs::EntityRef VrOrigin = ecs::Name("vr", "hmd");

    const ecs::EntityRef &Head() {
        if (VrHmd.GetLive() || (VrHmd.GetStaging() && !Flatview.GetLive())) {
            return VrHmd;
        }
        return Flatview;
    }

} // namespace sp::entities
