#pragma once

#include "Common.hh"

#include <ecs/Components.hh>

namespace ecs {
    class LightGun {
    public:
        LightGun();

        bool hasLight;
    };

    static Component<LightGun> ComponentLightGun("lightGun");

    template<>
    bool Component<LightGun>::Load(Lock<Read<ecs::Name>> lock, LightGun &dst, const picojson::value &src);
} // namespace ecs
