#pragma once

#include "ecs/Components.hh"

namespace ecs {
    struct Screen {
        string textureName;
        glm::vec3 luminanceScale = glm::vec3(1);
    };

    static const StructMetadata MetadataScreen(typeid(Screen),
        StructField::New("target", &Screen::textureName),
        StructField::New("luminance", &Screen::luminanceScale));
    static Component<Screen> ComponentScreen("screen", MetadataScreen);
} // namespace ecs
