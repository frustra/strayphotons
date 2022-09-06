#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    enum class OpticType {
        Gel = 0,
        Mirror,
    };

    struct OpticalElement {
        OpticType type = OpticType::Gel;
        sp::color_t tint = glm::vec3(1);
    };

    static Component<OpticalElement> ComponentOpticalElement("optic",
        ComponentField::New("type", &OpticalElement::type, FieldAction::AutoApply),
        ComponentField::New("tint", &OpticalElement::tint, FieldAction::AutoApply));

    template<>
    bool Component<OpticalElement>::Load(const EntityScope &scope, OpticalElement &dst, const picojson::value &src);
} // namespace ecs
