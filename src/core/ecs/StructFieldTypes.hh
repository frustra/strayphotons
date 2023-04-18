#pragma once

#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "ecs/Components.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalExpression.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>
#include <type_traits>
#include <typeindex>

namespace picojson {
    class value;
}

namespace ecs {
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
        glm::uvec2,
        glm::uvec3,
        glm::uvec4,
        sp::color_t,
        sp::color_alpha_t,
        glm::quat,

        // Structs
        std::string,
        EntityRef,
        EventData,
        Transform,
        SignalExpression,
        EventBindingActions,
        PhysicsMaterial,
        std::vector<float>,
        std::vector<glm::vec2>,
        std::vector<std::string>,
        std::vector<SignalExpression>,
        std::vector<EventDest>,
        std::vector<AnimationState>,
        std::vector<PhysicsShape>,
        std::vector<PhysicsJoint>,
        std::vector<ScriptInstance>,
        std::vector<Sound>,
        std::optional<double>,
        std::optional<SignalExpression>,
        std::optional<PhysicsActorType>,
        robin_hood::unordered_map<std::string, double>,
        robin_hood::unordered_map<std::string, std::string>,
        robin_hood::unordered_map<std::string, SignalExpression>,
        robin_hood::unordered_map<std::string, PhysicsJoint>,
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

    namespace detail {
        template<typename ArgType, typename Func, typename T, typename... Tn>
        inline static auto GetComponentType(std::type_index type,
            std::conditional_t<std::is_const_v<std::remove_reference_t<ArgType>>, const void *, void *> arg,
            Func &&func) {
            if (type == std::type_index(typeid(T))) {
                if constexpr (std::is_pointer_v<ArgType>) {
                    if constexpr (std::is_const_v<T>) {
                        return std::invoke(func, (const T *)arg);
                    } else {
                        return std::invoke(func, (T *)arg);
                    }
                } else {
                    if constexpr (std::is_const_v<T>) {
                        return std::invoke(func, *(const T *)arg);
                    } else {
                        return std::invoke(func, *(T *)arg);
                    }
                }
            }
            if constexpr (sizeof...(Tn) > 0) {
                return GetComponentType<ArgType, Func, Tn...>(type, arg, std::forward<Func>(func));
            } else {
                Abortf("Type missing from FieldTypes definition: %s", type.name());
            }
        }

        template<typename ArgType, typename... AllComponentTypes, template<typename...> typename ECSType, typename Func>
        inline static auto GetComponentType(ECSType<AllComponentTypes...> *,
            std::type_index type,
            std::conditional_t<std::is_const_v<std::remove_reference_t<ArgType>>, const void *, void *> arg,
            Func &&func) {
            return GetComponentType<ArgType, Func, AllComponentTypes...>(type, arg, std::forward<Func>(func));
        }
    } // namespace detail

    template<typename Func, size_t I = 0>
    inline static auto GetFieldType(std::type_index type, Func &&func) {
        if (type == std::type_index(typeid(std::tuple_element_t<I, FieldTypes>))) {
            return std::invoke(func, (std::tuple_element_t<I, FieldTypes> *)nullptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, std::forward<Func>(func));
        } else {
            return detail::GetComponentType<int *>((ECS *)nullptr, type, nullptr, std::forward<Func>(func));
        }
    }

    template<typename Func, size_t I = 0>
    inline static auto GetFieldType(std::type_index type, void *ptr, Func &&func) {
        if (type == std::type_index(typeid(std::tuple_element_t<I, FieldTypes>))) {
            Assertf(ptr != nullptr, "GetFieldType provided with nullptr");
            return std::invoke(func, *(std::tuple_element_t<I, FieldTypes> *)ptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, ptr, std::forward<Func>(func));
        } else {
            return detail::GetComponentType<int &>((ECS *)nullptr, type, ptr, std::forward<Func>(func));
        }
    }

    template<typename Func, size_t I = 0>
    inline static auto GetFieldType(std::type_index type, const void *ptr, Func &&func) {
        if (type == std::type_index(typeid(std::tuple_element_t<I, FieldTypes>))) {
            Assertf(ptr != nullptr, "GetFieldType provided with nullptr");
            return std::invoke(func, *(const std::tuple_element_t<I, FieldTypes> *)ptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, ptr, std::forward<Func>(func));
        } else {
            return detail::GetComponentType<const int &>((ECS *)nullptr, type, ptr, std::forward<Func>(func));
        }
    }
}; // namespace ecs
