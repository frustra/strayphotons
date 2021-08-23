#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs {
    class Animation {
    public:
        void AnimateToState(size_t state);

        struct State {
            glm::vec3 pos;
            glm::vec3 scale;

            State() {}
            State(glm::vec3 pos, glm::vec3 scale) : pos(pos), scale(scale) {}
        };

        std::vector<State> states;
        size_t curState = 0;
        size_t prevState = 0;

        /**
         * the time it takes to animate to the given state
         */
        std::vector<double> animationTimes;
    };

    static Component<Animation> ComponentAnimation("animation");

    template<>
    bool Component<Animation>::Load(Lock<Read<ecs::Name>> lock, Animation &dst, const picojson::value &src);
} // namespace ecs
