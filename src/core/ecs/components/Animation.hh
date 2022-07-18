#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs {
    enum class InterpolationMode {
        Step,
        Linear,
        Cubic // Cubic Hermite spline, see
              // https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic
    };

    struct AnimationState {
        // the time it takes to animate to this state
        double delay = 0.0;

        glm::vec3 pos = glm::vec3(0);
        glm::vec3 scale = glm::vec3(1);

        // derivative vectors, used for cubic interpolation only
        glm::vec3 tangentPos = glm::vec3(0);
        glm::vec3 tangentScale = glm::vec3(0);

        AnimationState() {}
        AnimationState(glm::vec3 pos, glm::vec3 scale) : pos(pos), scale(scale) {}

        bool operator==(const AnimationState &other) const {
            return pos == other.pos && scale == other.scale && tangentPos == other.tangentPos &&
                   tangentScale == other.tangentScale;
        }
    };

    class Animation {
    public:
        std::vector<AnimationState> states;
        size_t targetState = 0;
        size_t currentState = 0;
        double timeUntilNextState = 0;

        InterpolationMode interpolation = InterpolationMode::Linear;
        float tension = 0.5f;

        int PlayDirection() const {
            return targetState < currentState ? -1 : targetState > currentState ? 1 : 0;
        }
    };

    static Component<Animation> ComponentAnimation("animation",
        ComponentField::New("states", &Animation::states),
        ComponentField::New("defaultState", &Animation::targetState),
        ComponentField::New("interpolation", &Animation::interpolation),
        ComponentField::New("tension", &Animation::tension));

    template<>
    bool Component<Animation>::Load(const EntityScope &scope, Animation &dst, const picojson::value &src);
} // namespace ecs
