#include "Animation.hh"

#include "assets/AssetHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    template<>
    bool Component<Animation>::Load(const EntityScope &scope, Animation &animation, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "states") {
                for (auto state : param.second.get<picojson::array>()) {
                    double delay = 1.0;
                    Animation::State tangents;
                    bool hasTangent = false;

                    for (auto stateParam : state.get<picojson::object>()) {
                        if (stateParam.first == "delay") {
                            delay = stateParam.second.get<double>();
                        } else if (stateParam.first == "translate_tangent") {
                            tangents.pos = sp::MakeVec3(stateParam.second);
                            hasTangent = true;
                        } else if (stateParam.first == "scale_tangent") {
                            tangents.scale = sp::MakeVec3(stateParam.second);
                            hasTangent = true;
                        }
                    }

                    Transform animationState;
                    if (!Component<Transform>::Load(scope, animationState, state)) {
                        sp::Abort("Couldn't parse animation state as Transform");
                    }
                    animation.states.emplace_back(animationState.GetPosition(), animationState.GetScale());
                    animation.animationTimes.emplace_back(delay);
                    if (hasTangent) {
                        animation.tangents.resize(animation.states.size());
                        animation.tangents.back() = tangents;
                    }
                }
            } else if (param.first == "defaultState") {
                animation.targetState = param.second.get<double>();
            } else if (param.first == "interpolation") {
                auto mode = param.second.get<string>();
                if (mode == "step") {
                    animation.interpolation = InterpolationMode::Step;
                } else if (mode == "linear") {
                    animation.interpolation = InterpolationMode::Linear;
                } else if (mode == "cubic") {
                    animation.interpolation = InterpolationMode::Cubic;
                }
            } else if (param.first == "tension") {
                animation.tension = param.second.get<double>();
            }
        }
        if (animation.targetState >= animation.states.size()) animation.targetState = animation.states.size() - 1;
        if (animation.targetState < 0) animation.targetState = 0;
        animation.currentState = animation.targetState;

        if (animation.interpolation == InterpolationMode::Cubic) {
            Assert(animation.tangents.size() == animation.states.size(), "must have one tangent per state");
        }
        return true;
    }
} // namespace ecs
