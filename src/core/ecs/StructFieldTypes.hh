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
}

namespace ecs {
    enum class FieldAction;
    struct EntityScope;
    struct AnimationState;
    struct EventBinding;
    enum class FocusLayer : uint8_t;
    enum class GuiTarget;
    enum class InterpolationMode;
    enum class OpticType;
    enum class PhysicsGroup : uint16_t;
    enum class PhysicsJointType;
    enum class ScenePriority;
    struct PhysicsShape;
    struct PhysicsJoint;
    class ScriptState;
    struct SignalExpression;
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
        sp::color_t,
        sp::color_alpha_t,
        glm::ivec2,
        glm::ivec3,
        glm::quat,

        // Structs
        std::string,
        EntityRef,
        Transform,
        std::vector<std::string>,
        std::vector<AnimationState>,
        std::vector<PhysicsShape>,
        std::vector<PhysicsJoint>,
        std::vector<ScriptState>,
        std::vector<Sound>,
        std::optional<double>,
        robin_hood::unordered_map<std::string, double>,
        robin_hood::unordered_map<std::string, SignalExpression>,
        robin_hood::unordered_map<std::string, std::vector<SignalExpression>>,
        robin_hood::unordered_map<std::string, std::vector<EventBinding>>,

        // Enums
        FocusLayer,
        GuiTarget,
        InterpolationMode,
        OpticType,
        PhysicsGroup,
        PhysicsJointType,
        ScenePriority,
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
