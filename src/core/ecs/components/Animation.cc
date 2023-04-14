#include "Animation.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <sstream>

namespace ecs {
    template<>
    void Component<Animation>::Apply(Animation &dst, const Animation &src, bool liveTarget) {
        if (liveTarget || dst.states.empty()) {
            dst.states = src.states;
        }
    }

    Animation::CurrNextState Animation::GetCurrNextState(double currentState, double targetState) const {
        double floorState;
        float completion = std::modf(currentState, &floorState);
        if (targetState >= currentState) {
            return Animation::CurrNextState{(size_t)floorState, (size_t)floorState + 1, completion, 1};
        } else if (completion == 0.0) {
            return Animation::CurrNextState{(size_t)floorState, (size_t)floorState - 1, 0.0f, -1};
        } else {
            return Animation::CurrNextState{(size_t)floorState + 1, (size_t)floorState, 1.0f - completion, -1};
        }
    }

    bool isNormal(glm::vec3 scale) {
        return std::isnormal(scale.x) && std::isnormal(scale.y) && std::isnormal(scale.z);
    }

    void Animation::UpdateTransform(Lock<ReadSignalsLock, Read<Animation>, Write<TransformTree>> lock, Entity ent) {
        if (!ent.Has<Animation, TransformTree>(lock)) return;

        auto &animation = ent.Get<Animation>(lock);
        if (animation.states.empty()) return;

        auto &transform = ent.Get<TransformTree>(lock);

        double currentState = SignalBindings::GetSignal(lock, ent, "animation_state");
        double targetState = SignalBindings::GetSignal(lock, ent, "animation_target");
        currentState = std::clamp(currentState, 0.0, animation.states.size() - 1.0);
        targetState = std::clamp(targetState, 0.0, animation.states.size() - 1.0);
        auto state = animation.GetCurrNextState(currentState, targetState);
        if (state.current >= animation.states.size()) state.current = animation.states.size() - 1;
        if (state.next >= animation.states.size()) state.next = animation.states.size() - 1;

        auto &currState = animation.states[state.current];
        auto &nextState = animation.states[state.next];

        glm::vec3 dPos, dScale;
        switch (animation.interpolation) {
        case InterpolationMode::Step:
            transform.pose.SetPosition(nextState.pos);
            if (!glm::any(glm::isinf(nextState.scale))) transform.pose.SetScale(nextState.scale);
            break;
        case InterpolationMode::Linear:
            dPos = nextState.pos - currState.pos;
            transform.pose.SetPosition(currState.pos + state.completion * dPos);

            dScale = nextState.scale - currState.scale;
            if (isNormal(dScale)) transform.pose.SetScale(currState.scale + state.completion * dScale);
            break;
        case InterpolationMode::Cubic:
            float tangentScale = state.direction * nextState.delay;

            auto t = state.completion;
            auto t2 = t * t;
            auto t3 = t2 * t;
            auto av1 = 2 * t3 - 3 * t2 + 1;
            auto at1 = tangentScale * (t3 - 2 * t2 + t);
            auto av2 = -2 * t3 + 3 * t2;
            auto at2 = tangentScale * (t3 - t2);

            auto pos = av1 * currState.pos + at1 * currState.tangentPos + av2 * nextState.pos +
                       at2 * nextState.tangentPos;
            transform.pose.SetPosition(pos);

            auto scale = av1 * currState.scale + at1 * currState.tangentScale + av2 * nextState.scale +
                         at2 * nextState.tangentScale;
            if (isNormal(scale)) transform.pose.SetScale(scale);
            break;
        }
    }
} // namespace ecs
