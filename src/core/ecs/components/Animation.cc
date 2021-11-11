#include "Animation.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    template<>
    bool Component<Animation>::Load(sp::Scene *scene, Animation &animation, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "states") {
                for (auto state : param.second.get<picojson::array>()) {
                    double delay = 1.0;
                    for (auto stateParam : state.get<picojson::object>()) {
                        if (stateParam.first == "delay") { delay = stateParam.second.get<double>(); }
                    }

                    Transform animationState;
                    if (!Component<Transform>::Load(scene, animationState, state)) {
                        sp::Abort("Couldn't parse animation state as Transform");
                    }
                    animation.states.emplace_back(animationState.GetPosition(), animationState.GetScale());
                    animation.animationTimes.emplace_back(delay);
                }
            } else if (param.first == "defaultState") {
                animation.targetState = param.second.get<double>();
            }
        }
        if (animation.targetState >= animation.states.size()) animation.targetState = animation.states.size() - 1;
        if (animation.targetState < 0) animation.targetState = 0;
        animation.currentState = animation.targetState;
        return true;
    }
} // namespace ecs
