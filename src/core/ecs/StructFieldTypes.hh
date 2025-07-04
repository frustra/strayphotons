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
        EventDataType,
        std::string,
        sp::InlineString<63>,
        EventName,
        EventString,
        size_t,
        VisibilityMask,
        sp::color_alpha_t,
        double,
        glm::mat3,
        EntityRef,
        NamedEntity,

        // Basic types
        bool,
        char,
        int32_t,
        uint32_t,
        sp::angle_t,
        Entity,

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
        glm::mat4,

        // Structs
        AnimationState,
        DynamicScriptDefinition,
        Event,
        EventBinding,
        EventBindingActions,
        EventDest,
        PhysicsJoint,
        PhysicsMaterial,
        PhysicsShape,
        ScriptDefinition,
        ScriptDefinitionBase,
        ScriptInstance,
        ScriptState,
        SignalExpression,
        SignalRef,
        Sound,
        sp::SceneRef,
        std::vector<float>,
        std::vector<glm::vec2>,
        std::vector<std::string>,
        std::vector<SignalExpression>,
        std::vector<EventDest>,
        std::vector<EventBinding>,
        std::vector<AnimationState>,
        std::vector<PhysicsJoint>,
        std::vector<PhysicsShape>,
        std::vector<ScriptInstance>,
        std::vector<Sound>,
        std::vector<EntityRef>,
        std::vector<std::pair<EntityRef, EntityRef>>,
        std::pair<EntityRef, EntityRef>,
        std::optional<double>,
        std::optional<EventData>,
        std::optional<SignalExpression>,
        std::optional<PhysicsActorType>,
        robin_hood::unordered_map<std::string, double>,
        robin_hood::unordered_map<EventName, std::string, sp::StringHash, sp::StringEqual>,
        robin_hood::unordered_map<std::string, SignalExpression>,
        robin_hood::unordered_map<std::string, PhysicsJoint>,
        robin_hood::unordered_map<std::string, std::vector<SignalExpression>>,
        robin_hood::unordered_map<EventName, std::vector<EventBinding>, sp::StringHash, sp::StringEqual>,

        // Enums
        FocusLayer,
        GuiTarget,
        InterpolationMode,
        PhysicsGroup,
        PhysicsActorType,
        PhysicsJointType,
        sp::ScenePriority,
        ScriptType,
        SoundType,
        // TriggerGroup, // Also a Component type
        TriggerShape,
        XrEye,

        // Locks
        DynamicLock<>,
        DynamicLock<ReadSignalsLock>,
        DynamicLock<SendEventsLock>,
        Lock<>,
        Lock<Read<EventInput>>,
        Lock<Read<Signals>>,
        Lock<Read<TransformTree>>,
        Lock<Write<Signals>>,
        Lock<Write<Signals>, ReadSignalsLock>,
        Lock<Write<TransformTree>>,

        // Function pointers
        void *(*)(const void *),
        void (*)(void *),
        void (*)(void *, ScriptState *),
        void (*)(void *, ScriptState *),
        void (*)(void *, ScriptState *, DynamicLock<> *, Entity, uint64_t),
        void (*)(void *, ScriptState *, DynamicLock<> *, Entity, Event *),
        void (*)(const ScriptState *, DynamicLock<> *, Entity, const sp::SceneRef *)>;

    namespace detail {
        template<typename Func, typename T, typename... Tn>
        inline static auto GetComponentType(std::type_index type, Func &&func) {
            if (type == std::type_index(typeid(T))) return std::invoke(func, (T *)nullptr);
            if constexpr (sizeof...(Tn) > 0) {
                return GetComponentType<Func, Tn...>(type, std::forward<Func>(func));
            } else {
                Abortf("Type missing from FieldTypes definition: %s", type.name());
            }
        }

        template<typename... AllComponentTypes, template<typename...> typename ECSType, typename Func>
        inline static auto GetComponentType(ECSType<AllComponentTypes...> *, std::type_index type, Func &&func) {
            return GetComponentType<Func, AllComponentTypes...>(type, std::forward<Func>(func));
        }
    } // namespace detail

    template<typename Func>
    inline static auto GetComponentType(std::type_index type, Func &&func) {
        return detail::GetComponentType((ECS *)nullptr, type, std::forward<Func>(func));
    }

    template<typename Func, size_t I = 0>
    inline static auto GetFieldType(std::type_index type, Func &&func) {
        if (type == std::type_index(typeid(std::tuple_element_t<I, FieldTypes>))) {
            return std::invoke(func, (std::tuple_element_t<I, FieldTypes> *)nullptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, std::forward<Func>(func));
        } else {
            return detail::GetComponentType((ECS *)nullptr, type, std::forward<Func>(func));
        }
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
