#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct OpticalElement {
        sp::color_t passTint = glm::vec3(0);
        sp::color_t reflectTint = glm::vec3(1);
        bool singleDirection = false;
    };

    static StructMetadata MetadataOpticalElement(typeid(OpticalElement),
        "optic",
        "",
        StructField::New("pass_tint", &OpticalElement::passTint),
        StructField::New("reflect_tint", &OpticalElement::reflectTint),
        StructField::New("single_direction", &OpticalElement::singleDirection));
    static Component<OpticalElement> ComponentOpticalElement(MetadataOpticalElement);
} // namespace ecs
