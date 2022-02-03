#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs {
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

        /**
         * the time it takes to animate to the given state
         */
        std::vector<double> animationTimes;

        size_t PlayDirection() const {
            return targetState < currentState ? -1 : targetState > currentState ? 1 : 0;
        }
    };

    static Component<Animation> ComponentAnimation("animation");

    template<>
    bool Component<Animation>::Load(sp::Scene *scene, Animation &dst, const picojson::value &src);
} // namespace ecs
