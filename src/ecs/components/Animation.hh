#pragma once

#include "Common.hh"

#include <ecs/Components.hh>
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
            State(glm::vec3 pos, glm::vec3 scale, bool hidden = false) : pos(pos), scale(scale), hidden(hidden) {}

            // if model should be hidden upon reaching this state
            bool hidden = false;
        };

        vector<State> states;
        size_t curState = 0;
        size_t prevState = 0;

        /**
         * the time it takes to animate to the given state
         */
        vector<double> animationTimes;
    };

    static Component<Animation> ComponentAnimation("animation");

    template<>
    bool Component<Animation>::Load(Lock<Read<ecs::Name>> lock, Animation &dst, const picojson::value &src);
} // namespace ecs
