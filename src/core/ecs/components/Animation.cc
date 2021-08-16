#include "Animation.hh"

#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <ecs/components/Transform.hh>
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    template<>
    bool Component<Animation>::Load(Lock<Read<ecs::Name>> lock, Animation &animation, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "states") {
                for (auto state : param.second.get<picojson::array>()) {
                    double delay = 1.0;
                    for (auto stateParam : state.get<picojson::object>()) {
                        if (stateParam.first == "delay") { delay = stateParam.second.get<double>(); }
                    }

                    Transform animationState;
                    if (!Component<Transform>::Load(lock, animationState, state)) {
                        throw std::runtime_error("Couldn't parse animation state as Transform");
                        return false;
                    }
                    animation.states.emplace_back(animationState.GetPosition(), animationState.GetScaleVec());
                    animation.animationTimes.emplace_back(delay);
                }
            } else if (param.first == "defaultState") {
                animation.curState = param.second.get<double>();
            }
        }
        if (animation.curState < 0) animation.curState = 0;
        animation.prevState = animation.curState;
        return true;
    }

    void Animation::AnimateToState(size_t state) {
        if (state >= states.size()) {
            std::stringstream ss;
            ss << "\"" << state << "\" is an invalid state for this Animation with " << states.size() << " states";
            throw std::runtime_error(ss.str());
        }
        if (state == curState) return;

        prevState = curState;
        curState = state;
    }
} // namespace ecs
