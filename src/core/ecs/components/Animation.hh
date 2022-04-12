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

    class Animation {
    public:
        struct State {
            glm::vec3 pos;
            glm::vec3 scale;

            State() {}
            State(glm::vec3 pos, glm::vec3 scale) : pos(pos), scale(scale) {}
        };

        std::vector<State> states;
        size_t targetState = 0;
        size_t currentState = 0;
        double timeUntilNextState = 0;

        // the time it takes to animate to the given state
        std::vector<double> animationTimes;

        // derivative vectors at each state, used for cubic interpolation only
        std::vector<State> tangents;

        InterpolationMode interpolation = InterpolationMode::Linear;
        float tension = 0.5f;

        int PlayDirection() const {
            return targetState < currentState ? -1 : targetState > currentState ? 1 : 0;
        }
    };

    static Component<Animation> ComponentAnimation("animation");

    template<>
    bool Component<Animation>::Load(const EntityScope &scope, Animation &dst, const picojson::value &src);
} // namespace ecs
