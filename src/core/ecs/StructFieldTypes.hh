#pragma once

#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpression.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>
#include <type_traits>
#include <typeindex>

namespace picojson {
    class value;
}

namespace sp {
    class Gltf;
    enum class ScenePriority;
} // namespace sp

namespace ecs {
    enum class FieldAction;
    struct AnimationState;
    struct EventBinding;
    enum class FocusLayer;
    enum class GuiTarget;
    enum class InterpolationMode;
    enum class PhysicsGroup : uint16_t;
    enum class PhysicsActorType : uint8_t;
    enum class PhysicsJointType;
    struct PhysicsShape;
    struct PhysicsJoint;
    class ScriptState;
    class SignalExpression;
    enum class SoundType;
    class Sound;
    enum class TriggerShape : uint8_t;
    enum class VisibilityMask;
    enum class XrEye;

    using FieldTypes = std::tuple<
        // Basic types
        bool,
        int32_t,
        uint32_t,
        size_t,
        sp::angle_t,
        float,
        double,

        // Vector types
        glm::vec2,
        glm::vec3,
        glm::vec4,
        glm::dvec2,
        glm::dvec3,
        glm::dvec4,
        glm::ivec2,
        glm::ivec3,
        glm::ivec4,
        sp::color_t,
        sp::color_alpha_t,
        glm::quat,

        // Structs
        std::string,
        EntityRef,
        Transform,
        SignalExpression,
        std::vector<float>,
        std::vector<glm::vec2>,
        std::vector<std::string>,
        std::vector<AnimationState>,
        std::vector<PhysicsShape>,
        std::vector<PhysicsJoint>,
        std::vector<ScriptState>,
        std::vector<Sound>,
        std::optional<double>,
        std::optional<SignalExpression>,
        std::optional<PhysicsActorType>,
        robin_hood::unordered_map<std::string, double>,
        robin_hood::unordered_map<std::string, SignalExpression>,
        robin_hood::unordered_map<std::string, std::vector<SignalExpression>>,
        robin_hood::unordered_map<std::string, std::vector<EventBinding>>,

        // Enums
        FocusLayer,
        GuiTarget,
        InterpolationMode,
        PhysicsGroup,
        PhysicsActorType,
        PhysicsJointType,
        sp::ScenePriority,
        SoundType,
        TriggerGroup,
        TriggerShape,
        VisibilityMask,
        XrEye>;

    template<typename Func, size_t I = 0>
    inline static auto GetFieldType(std::type_index type, Func func) {
        if (type == std::type_index(typeid(std::tuple_element_t<I, FieldTypes>))) {
            return std::invoke(func, (std::tuple_element_t<I, FieldTypes> *)nullptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, func);
        } else {
            Abortf("Type missing from FieldTypes definition: %s", type.name());
        }
    }
}; // namespace ecs
