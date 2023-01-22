#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct OpticalElement {
        sp::color_t passTint = glm::vec3(0);
        sp::color_t reflectTint = glm::vec3(1);
    };

    static StructMetadata MetadataOpticalElement(typeid(OpticalElement),
        StructField::New("pass_tint", &OpticalElement::passTint),
        StructField::New("reflect_tint", &OpticalElement::reflectTint));
    static Component<OpticalElement> ComponentOpticalElement("optic", MetadataOpticalElement);
} // namespace ecs
