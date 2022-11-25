#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    enum class OpticType {
        Gel = 0,
        Mirror,
        Splitter, // Tint defines reflected color
    };

    struct OpticalElement {
        OpticType type = OpticType::Gel;
        sp::color_t tint = glm::vec3(1);
    };

    static Component<OpticalElement> ComponentOpticalElement("optic",
        ComponentField::New("type", &OpticalElement::type),
        ComponentField::New("tint", &OpticalElement::tint));
} // namespace ecs
