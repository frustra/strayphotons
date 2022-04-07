#pragma once

#include "ecs/Components.hh"

namespace ecs {
    struct Screen {
        string textureName;
        glm::vec3 luminanceScale = {1.0f, 1.0f, 1.0f};
    };

    static Component<Screen> ComponentScreen("screen");

    template<>
    bool Component<Screen>::Load(ScenePtr scenePtr, const Name &scope, Screen &dst, const picojson::value &src);
} // namespace ecs
