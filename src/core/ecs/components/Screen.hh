#pragma once

#include "ecs/Components.hh"

namespace ecs {
    struct Screen {
        string textureName;
        glm::vec3 luminanceScale = glm::vec3(1);
    };

    static Component<Screen> ComponentScreen("screen",
        ComponentField::New("target", &Screen::textureName),
        ComponentField::New("luminance", &Screen::luminanceScale));
} // namespace ecs
