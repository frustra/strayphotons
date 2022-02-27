#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    struct Mirror {
        glm::vec2 size;
        int mirrorId;
    };

    static Component<Mirror> ComponentMirror("mirror");

    template<>
    bool Component<Mirror>::Load(ScenePtr scenePtr, Mirror &dst, const picojson::value &src);
} // namespace ecs
