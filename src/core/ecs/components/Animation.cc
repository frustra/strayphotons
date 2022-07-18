#include "Animation.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <sstream>

namespace sp::json {
    template<>
    bool Load(const ecs::EntityScope &scope, ecs::AnimationState &state, const picojson::value &src) {
        if (!src.is<picojson::object>()) {
            Errorf("Invalid animation state: %s", src.to_str());
            return false;
        }

        for (auto &param : src.get<picojson::object>()) {
            if (param.first == "delay") {
                if (!sp::json::Load(scope, state.delay, param.second)) {
                    Errorf("Invalid animation delay: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "scale") {
                if (!sp::json::Load(scope, state.scale, param.second)) {
                    Errorf("Invalid animation scale: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "translate") {
                if (!sp::json::Load(scope, state.pos, param.second)) {
                    Errorf("Invalid animation translation: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "scale_tangent") {
                if (!sp::json::Load(scope, state.tangentScale, param.second)) {
                    Errorf("Invalid animation scale_tangent: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "translate_tangent") {
                if (!sp::json::Load(scope, state.tangentPos, param.second)) {
                    Errorf("Invalid animation translate_tangent: %s", param.second.to_str());
                    return false;
                }
            }
        }
        return true;
    }

    template<>
    void Save(const ecs::EntityScope &scope, picojson::value &dst, const ecs::AnimationState &src) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        static const ecs::AnimationState defaultState = {};

        SaveIfChanged(scope, obj, "delay", src.delay, defaultState.delay);
        SaveIfChanged(scope, obj, "translate", src.pos, defaultState.pos);
        SaveIfChanged(scope, obj, "scale", src.scale, defaultState.scale);
        SaveIfChanged(scope, obj, "translate_tangent", src.tangentPos, defaultState.tangentPos);
        SaveIfChanged(scope, obj, "scale_tangent", src.tangentScale, defaultState.tangentScale);
    }
} // namespace sp::json

namespace ecs {
    template<>
    bool Component<Animation>::Load(const EntityScope &scope, Animation &animation, const picojson::value &src) {
        if (animation.targetState >= animation.states.size()) animation.targetState = animation.states.size() - 1;
        if (animation.targetState < 0) animation.targetState = 0;
        animation.currentState = animation.targetState;
        return true;
    }
} // namespace ecs
