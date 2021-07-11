#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
    struct Light {
        float spotAngle, intensity, illuminance;
        glm::vec3 tint;
        glm::vec4 mapOffset;
        int gelId;
        int lightId;
        bool on = true;
        Tecs::Entity bulb;
    };

    static Component<Light> ComponentLight("light");

    template<>
    bool Component<Light>::Load(Lock<Read<ecs::Name>> lock, Light &dst, const picojson::value &src);
} // namespace ecs
