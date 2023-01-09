#include "Animation.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <sstream>

namespace ecs {
    template<>
    bool StructMetadata::Load<Animation>(const EntityScope &scope, Animation &animation, const picojson::value &src) {
        if (animation.targetState >= animation.states.size()) animation.targetState = animation.states.size() - 1;
        if (animation.targetState < 0) animation.targetState = 0;
        animation.currentState = animation.targetState;
        return true;
    }

    template<>
    void Component<Animation>::Apply(Animation &dst, const Animation &src, bool liveTarget) {
        if (liveTarget || dst.states.empty()) {
            dst.states = src.states;
        }
    }
} // namespace ecs
