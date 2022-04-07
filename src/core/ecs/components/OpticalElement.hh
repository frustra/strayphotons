#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    enum class OpticType {
        Gel = 0,
        Mirror,
        Count,
    };

    struct OpticalElement {
        OpticType type = OpticType::Gel;
        glm::vec3 tint = glm::vec3(1);
    };

    static Component<OpticalElement> ComponentOpticalElement("optic");

    template<>
    bool Component<OpticalElement>::Load(ScenePtr scenePtr,
        const Name &scope,
        OpticalElement &dst,
        const picojson::value &src);
} // namespace ecs
