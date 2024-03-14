/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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

    static const StructMetadata::EnumDescriptions DocsEnumInterpolationMode = {
        {(uint32_t)InterpolationMode::Step, "Teleport entities from state to state."},
        {(uint32_t)InterpolationMode::Linear, "Move entities at a constant speed between states."},
        {(uint32_t)InterpolationMode::Cubic, "Move entities according to a customizable Cubic Hermite spline curve."},
    };
    static const StructMetadata MetadataInterpolationMode(typeid(InterpolationMode),
        "InterpolationMode",
        "",
        &DocsEnumInterpolationMode);

    static const char *DocsDescriptionAnimation = R"(
Animations control the position of an entity by moving it between a set of animation states. Animation updates happen in the physics thread before each simulation step.
When an animation state is defined, the `transform` position is ignored except for the transform parent, using the pose from the animation.

Animations read and write two signal values:
1. **animation_state** - The current state index represented as a double from `0.0` to `N-1.0`.  
    A state value of `0.5` represents a state half way between states 0 and 1 based on transition time.
2. **animation_target** - The target state index. The entity will always animate towards this state.

The animation is running any time these values are different, and paused when they are equal.
)";

    static const char *DocsDescriptionAnimationState = R"(
An example of a 3-state linear animation might look like this:
```json
"animation": {
    "states": [
        {
            "delay": 0.5,
            "translate": [0, 0, 0]
        },
        {
            "delay": 0.5,
            "translate": [0, 1, 0]
        },
        {
            "delay": 0.5,
            "translate": [0, 0, 1]
        }
    ]
}
```

When moving from state `2.0` to state `0.0`, the animation will follow the path through state `1.0`, rather than moving directly to the target position. The `animation_state` signal can however be manually controlled to teleport the animation to a specific state.
)";

    struct AnimationState {
        // the time it takes to animate to this state
        double delay = 0.0;

        glm::vec3 pos = glm::vec3(0);
        glm::vec3 scale = glm::vec3(0);

        // derivative vectors, used for cubic interpolation only
        glm::vec3 tangentPos = glm::vec3(0);
        glm::vec3 tangentScale = glm::vec3(0);

        AnimationState() {}
        AnimationState(glm::vec3 pos, glm::vec3 scale) : pos(pos), scale(scale) {}

        bool operator==(const AnimationState &) const = default;
    };

    static StructMetadata MetadataAnimationState(typeid(AnimationState),
        "AnimationState",
        DocsDescriptionAnimationState,
        StructField::New("delay",
            "The time it takes to move to this state from any other state (in seconds)",
            &AnimationState::delay),
        StructField::New("translate", "A new position to override this entity's `transform`", &AnimationState::pos),
        StructField::New("scale",
            "A new scale to override this entity's `transform`. A scale of 0 will leave the transform unchanged.",
            &AnimationState::scale),
        StructField::New("translate_tangent",
            "Cubic interpolation tangent vector for **translate** (represents speed)",
            &AnimationState::tangentPos),
        StructField::New("scale_tangent",
            "Cubic interpolation tangent vector for **scale** (represents rate of scaling)",
            &AnimationState::tangentScale));

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
        DocsDescriptionAnimation,
        StructField::New("states", &Animation::states, ~FieldAction::AutoApply),
        StructField::New("interpolation", &Animation::interpolation),
        StructField::New("cubic_tension", &Animation::tension));
    static Component<Animation> ComponentAnimation(MetadataAnimation);

    template<>
    void Component<Animation>::Apply(Animation &dst, const Animation &src, bool liveTarget);
} // namespace ecs
