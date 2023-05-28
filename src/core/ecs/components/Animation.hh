#pragma once

#include "ecs/Components.hh"
#include "ecs/SignalExpression.hh"

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
        glm::vec3 scale = glm::vec3(-INFINITY);

        // derivative vectors, used for cubic interpolation only
        glm::vec3 tangentPos = glm::vec3(0);
        glm::vec3 tangentScale = glm::vec3(0);

        AnimationState() {}
        AnimationState(glm::vec3 pos, glm::vec3 scale) : pos(pos), scale(scale) {}

        bool operator==(const AnimationState &) const = default;
    };

    static StructMetadata MetadataAnimationState(typeid(AnimationState),
        "AnimationState",
        StructField::New("delay", &AnimationState::delay),
        StructField::New("translate", &AnimationState::pos),
        StructField::New("scale", &AnimationState::scale),
        StructField::New("translate_tangent", &AnimationState::tangentPos),
        StructField::New("scale_tangent", &AnimationState::tangentScale));

    class Animation {
    public:
        std::vector<AnimationState> states;

        InterpolationMode interpolation = InterpolationMode::Linear;
        float tension = 0.5f;

        struct CurrNextState {
            size_t current, next;
            float completion;
            int direction;
        };

        CurrNextState GetCurrNextState(double currentState, double targetState) const;
        static void UpdateTransform(
            Lock<ReadSignalsLock, Read<Animation, ecs::LightSensor, ecs::LaserSensor>, Write<TransformTree>> lock,
            Entity ent);
    };

    static StructMetadata MetadataAnimation(typeid(Animation),
        "animation",
        StructField::New("states", &Animation::states, ~FieldAction::AutoApply),
        StructField::New("interpolation", &Animation::interpolation),
        StructField::New("cubic_tension", &Animation::tension));
    static Component<Animation> ComponentAnimation(MetadataAnimation);

    template<>
    void Component<Animation>::Apply(Animation &dst, const Animation &src, bool liveTarget);
} // namespace ecs
