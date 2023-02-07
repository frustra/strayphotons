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

    struct AnimationState {
        // the time it takes to animate to this state
        double delay = 0.0;

        glm::vec3 pos = glm::vec3(0);
        glm::vec3 scale = glm::vec3(1);

        // derivative vectors, used for cubic interpolation only
        glm::vec3 tangentPos = glm::vec3(0);
        glm::vec3 tangentScale = glm::vec3(0);

        AnimationState() {}
        AnimationState(glm::vec3 pos, glm::vec3 scale) : pos(pos), scale(scale) {}

        bool operator==(const AnimationState &) const = default;
    };

    static StructMetadata MetadataAnimationState(typeid(AnimationState),
        StructField::New("delay", &AnimationState::delay),
        StructField::New("translate", &AnimationState::pos),
        StructField::New("scale", &AnimationState::scale),
        StructField::New("translate_tangent", &AnimationState::tangentPos),
        StructField::New("scale_tangent", &AnimationState::tangentScale));

    class Animation {
    public:
        std::vector<AnimationState> states;
        size_t targetState = 0;
        size_t currentState = 0;
        double timeUntilNextState = 0;
        double realState = 0;

        InterpolationMode interpolation = InterpolationMode::Linear;
        float tension = 0.5f;

        int PlayDirection() const {
            return targetState < currentState ? -1 : targetState > currentState ? 1 : 0;
        }
    };

    static StructMetadata MetadataAnimation(typeid(Animation),
        StructField::New("states", &Animation::states, ~FieldAction::AutoApply),
        StructField::New("defaultState", &Animation::targetState),
        StructField::New("interpolation", &Animation::interpolation),
        StructField::New("tension", &Animation::tension),
        StructField::New("state", &Animation::realState, FieldAction::None));
    static Component<Animation> ComponentAnimation("animation", MetadataAnimation);

    template<>
    bool StructMetadata::Load<Animation>(const EntityScope &scope, Animation &dst, const picojson::value &src);
    template<>
    void Component<Animation>::Apply(Animation &dst, const Animation &src, bool liveTarget);
} // namespace ecs
