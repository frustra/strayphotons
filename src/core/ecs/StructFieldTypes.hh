/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/EnumTypes.hh"
#include "ecs/Components.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>
#include <type_traits>
#include <typeindex>

namespace picojson {
    class value;
}

namespace ecs {
    using FieldTypes = std::tuple<
        // Top types based on counts in Tracy
        float,
        glm::vec2,
        glm::vec3,
        Transform,
        EventData,
        std::string,
        size_t,
        VisibilityMask,
        sp::color_alpha_t,
        double,
        glm::mat3,
        EntityRef,

        // Basic types
        bool,
        int32_t,
        uint32_t,
        sp::angle_t,

        // Vector types
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
        glm::quat,

        // Structs
        SignalExpression,
        EventBinding,
        EventBindingActions,
        EventDest,
        AnimationState,
        PhysicsJoint,
        PhysicsMaterial,
        PhysicsShape,
        ScriptInstance,
        Sound,
        std::vector<float>,
        std::vector<glm::vec2>,
        std::vector<std::string>,
        std::vector<SignalExpression>,
        std::vector<EventDest>,
        std::vector<AnimationState>,
        std::vector<PhysicsJoint>,
        std::vector<PhysicsShape>,
        std::vector<ScriptInstance>,
        std::vector<Sound>,
        std::optional<double>,
        std::optional<EventData>,
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
        TriggerShape,
        XrEye>;

    namespace detail {
        template<typename Wrapper1, typename Wrapper2>
        struct WrapperConcat;

        template<typename... Ta,
            template<typename...> typename Wrapper1,
            typename... Tb,
            template<typename...> typename Wrapper2>
        struct WrapperConcat<Wrapper1<Ta...>, Wrapper2<Tb...>> {
            using type = Wrapper1<Ta..., Tb...>;
        };

        template<typename T, typename... Tn>
        struct SelectFirstType {
            using type = T;
        };

        template<typename Wrapper>
        struct TypeLookup;

        template<typename... Tn, template<typename...> typename Wrapper>
        struct TypeLookup<Wrapper<Tn...>> {
            template<typename Func>
            static inline auto GetType(std::type_index type, Func &&func) {
                ZoneScopedN("GetType");
                ZoneStr(type.name());
                using T1 = SelectFirstType<Tn...>::type;
                using ReturnT = std::invoke_result_t<Func, T1 *>;
                constexpr bool hasReturn = !std::is_same_v<ReturnT, void>;
                using ResultT = std::conditional_t<hasReturn, ReturnT, bool>;
                std::optional<ResultT> result;
                (
                    [&] {
                        if (type == std::type_index(typeid(Tn))) {
                            if constexpr (hasReturn) {
                                result = std::invoke(func, (Tn *)nullptr);
                            } else {
                                std::invoke(func, (Tn *)nullptr);
                                result = true;
                            }
                        }
                    }(),
                    ...);
                Assertf(result.has_value(), "TypeLookup::GetType received unknown type: %s", type.name());
                if constexpr (hasReturn) {
                    return *result;
                }
            }
        };
    } // namespace detail

    template<typename Func>
    inline static auto GetFieldType(std::type_index type, Func &&func) {
        using AllTypes = detail::WrapperConcat<FieldTypes, ECS>::type;
        return detail::TypeLookup<AllTypes>::GetType(type, std::forward<Func>(func));
    }

    template<typename Func>
    inline static auto GetComponentType(std::type_index type, Func &&func) {
        return detail::TypeLookup<ECS>::GetType(type, std::forward<Func>(func));
    }

    template<typename ArgType, typename Func>
    inline static auto GetFieldType(const std::type_index &type, ArgType *ptr, Func &&func) {
        return GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            if constexpr (std::is_const_v<ArgType>) {
                return func(*reinterpret_cast<const T *>(ptr));
            } else {
                return func(*reinterpret_cast<T *>(ptr));
            }
        });
    }
} // namespace ecs
