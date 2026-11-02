/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"
#include "strayphotons/FlatSet.hh"
#include "strayphotons/Hashing.hh"
#include "strayphotons/HeapString.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/InlineString.hh"
#include "strayphotons/Utility.hh"
#include "strayphotons/gui/GuiDrawData.hh"
#include "strayphotons/gui/GuiInputEvents.hh"
#include "strayphotons/input/KeyCodes.hh"

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <robin_hood.h>
#include <tuple>
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
        sp::HeapString,
        sp::InlineString<63>,
        EventName,
        EventString,
        EventBytes,
        uint64_t, // size_t
        VisibilityMask,
        sp::color_alpha_t,
        double,
        glm::mat3,
        EntityRef,
        NamedEntity,

        // Basic types
        bool,
        char,
        uint8_t,
        uint16_t,
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
        StructField,
        sp::GenericCompositor,
        sp::GuiDrawData,
        sp::GuiDrawCommand,
        sp::GuiDrawVertex,
        sp::GuiInputEvent,
        sp::GuiInputMouseButtonEvent,
        sp::GuiInputKeyEvent,
        sp::SceneRef,
        TypeInfo,
        sp::InlineString<2>,
        sp::HeapVector<uint8_t>,
        sp::HeapVector<float>,
        sp::HeapVector<glm::vec2>,
        sp::HeapVector<sp::HeapString>,
        sp::HeapVector<sp::InlineString<127>>,
        sp::HeapVector<sp::InlineString<255>>,
        sp::HeapVector<SignalExpression>,
        sp::HeapVector<EventDest>,
        sp::HeapVector<EventBinding>,
        sp::HeapVector<sp::GuiDrawCommand>,
        sp::HeapVector<sp::GuiDrawIndex>,
        sp::HeapVector<sp::GuiDrawVertex>,
        sp::HeapVector<sp::GuiInputEvent>,
        sp::HeapVector<AnimationState>,
        sp::HeapVector<PhysicsJoint>,
        sp::HeapVector<PhysicsShape>,
        sp::HeapVector<ScriptInstance>,
        sp::HeapVector<Sound>,
        sp::HeapVector<StructField>,
        sp::HeapVector<Entity>,
        sp::HeapVector<EntityRef>,
        sp::HeapVector<std::pair<EntityRef, EntityRef>>,
        sp::EntityMap<float>,
        sp::FlatSet<Entity>,
        std::pair<EntityRef, EntityRef>,
        std::pair<Entity, float>,
        std::optional<double>,
        std::optional<EventData>,
        std::optional<SignalExpression>,
        std::optional<PhysicsActorType>,
        robin_hood::unordered_map<sp::InlineString<127>, double, sp::StringHash, sp::StringEqual>,
        robin_hood::unordered_map<EventName, EventName, sp::StringHash, sp::StringEqual>,
        robin_hood::unordered_map<sp::InlineString<127>, SignalExpression, sp::StringHash, sp::StringEqual>,
        robin_hood::unordered_map<sp::HeapString, SignalExpression, sp::StringHash, sp::StringEqual>,
        robin_hood::unordered_map<sp::InlineString<127>, PhysicsJoint, sp::StringHash, sp::StringEqual>,
        robin_hood::
            unordered_map<sp::InlineString<63>, sp::HeapVector<SignalExpression>, sp::StringHash, sp::StringEqual>,
        robin_hood::unordered_map<EventName, sp::HeapVector<EventBinding>, sp::StringHash, sp::StringEqual>,

        // Enums
        FieldAction,
        FocusLayer,
        sp::GuiInputEventType,
        sp::GuiInputSource,
        sp::GuiMouseSource,
        sp::MouseButton,
        sp::KeyCode,
        sp::InputAction,
        GuiLayoutAnchor,
        InterpolationMode,
        PhysicsGroup,
        PhysicsActorType,
        PhysicsJointType,
        sp::ScenePriority,
        SoundType,
        // TriggerGroup, // Also a Component type
        TriggerShape,
        XrEye,

        // Locks
        DynamicLock<>,
        DynamicLock<ReadSignalsLock>,
        DynamicLock<SendEventsLock>,
        Lock<>,
        Lock<Read<Signals>>,
        Lock<Read<TransformTree>>,
        Lock<Write<Signals>>,
        Lock<Write<Signals>, ReadSignalsLock>,
        Lock<Write<TransformTree>>,

        void *,

        // Function pointers
        void *(*)(const void *),
        void (*)(void *),
        void (*)(void *, ScriptState &),
        void (*)(void *, ScriptState &, DynamicLock<> &, Entity, uint64_t), // OnLogicTick/OnPhysicsTick
        void (*)(void *, ScriptState &, DynamicLock<> &, Entity, Event &), // OnEvent
        void (*)(const ScriptState &, DynamicLock<> &, Entity, const sp::SceneRef &), // RunPrefab
        bool (*)(void *,
            sp::GenericCompositor &,
            ScriptState &,
            Entity,
            const sp::HeapVector<sp::GuiInputEvent> &), // BeforeFrame
        void (*)(void *,
            sp::GenericCompositor &,
            ScriptState &,
            Entity,
            glm::vec2,
            glm::vec2,
            float,
            sp::GuiDrawData &) // RenderGui
        >;

    namespace detail {
        template<typename T0, typename... Tn, template<typename...> typename ECSType, typename Func>
        inline static auto GetComponentType(ECSType<T0, Tn...> *, const std::type_index &type, Func &&func) {
            using ResultType = std::invoke_result_t<Func, T0 *>;
            if constexpr (std::is_same<ResultType, void>()) {
                if (type == typeid(T0)) {
                    std::invoke(func, (T0 *)nullptr);
                } else {
                    bool found = ((type == typeid(Tn) ? (std::invoke(func, (Tn *)nullptr), true) : false) || ...);
                    Assertf(found, "Type missing from FieldTypes definition: %s", type.name());
                }
            } else {
                if (type == typeid(T0)) {
                    return std::invoke(func, (T0 *)nullptr);
                } else {
                    std::optional<ResultType> result;
                    bool found = ((type == typeid(Tn) ? (result = std::invoke(func, (Tn *)nullptr))
                                                      : std::optional<ResultType>{}) ||
                                  ...);
                    Assertf(found, "Type missing from FieldTypes definition: %s", type.name());
                    return *result;
                }
            }
        }

        template<typename T0,
            typename... Tn,
            typename... AllComponentTypes,
            template<typename...> typename TupleType,
            template<typename...> typename ECSType,
            typename Func>
        inline static auto GetFieldType(TupleType<T0, Tn...> *,
            ECSType<AllComponentTypes...> *,
            const std::type_index &type,
            Func &&func) {
            using ResultType = std::invoke_result_t<Func, T0 *>;
            if constexpr (std::is_same<ResultType, void>()) {
                if (type == typeid(T0)) {
                    std::invoke(func, (T0 *)nullptr);
                } else {
                    bool found = ((type == typeid(Tn) ? (std::invoke(func, (Tn *)nullptr), true) : false) || ...) ||
                                 ((type == typeid(AllComponentTypes)
                                          ? (std::invoke(func, (AllComponentTypes *)nullptr), true)
                                          : false) ||
                                     ...);
                    Assertf(found, "Type missing from FieldTypes definition: %s", type.name());
                }
            } else {
                std::optional<ResultType> result;
                if (type == typeid(T0)) {
                    return std::invoke(func, (T0 *)nullptr);
                } else {
                    bool found = ((type == typeid(Tn) ? (result = std::invoke(func, (Tn *)nullptr))
                                                      : std::optional<ResultType>{}) ||
                                     ...) ||
                                 ((type == typeid(AllComponentTypes)
                                          ? (result = std::invoke(func, (AllComponentTypes *)nullptr))
                                          : std::optional<ResultType>{}) ||
                                     ...);
                    Assertf(found, "Type missing from FieldTypes definition: %s", type.name());
                    return *result;
                }
            }
        }

        template<typename T0,
            typename... Tn,
            typename... AllComponentTypes,
            template<typename...> typename TupleType,
            template<typename...> typename ECSType,
            typename Func>
        inline static auto GetFieldType(TupleType<T0, Tn...> *,
            ECSType<AllComponentTypes...> *,
            uint32_t typeIndex,
            Func &&func) {
            using ResultType = std::invoke_result_t<Func, T0 *>;
            if constexpr (std::is_same<ResultType, void>()) {
                if (typeIndex == 1) {
                    std::invoke(func, (T0 *)nullptr);
                } else {
                    uint32_t i = 1;
                    bool found = (((typeIndex == ++i) ? (std::invoke(func, (Tn *)nullptr), true) : false) || ...) ||
                                 (((typeIndex == ++i) ? (std::invoke(func, (AllComponentTypes *)nullptr), true)
                                                      : false) ||
                                     ...);
                    Assertf(found, "Type missing from FieldTypes definition: %u", typeIndex);
                }
            } else {
                if (typeIndex == 1) {
                    return std::invoke(func, (T0 *)nullptr);
                } else {
                    std::optional<ResultType> result;
                    uint32_t i = 1;
                    bool found = (((typeIndex == ++i) ? (result = std::invoke(func, (Tn *)nullptr))
                                                      : std::optional<ResultType>{}) ||
                                     ...) ||
                                 (((typeIndex == ++i) ? (result = std::invoke(func, (AllComponentTypes *)nullptr))
                                                      : std::optional<ResultType>{}) ||
                                     ...);
                    Assertf(found, "Type missing from FieldTypes definition: %u", typeIndex);
                    return *result;
                }
            }
        }

        template<typename T,
            typename... Tn,
            typename... AllComponentTypes,
            template<typename...> typename TupleType,
            template<typename...> typename ECSType>
        inline static uint32_t GetFieldTypeIndex(TupleType<Tn...> *, ECSType<AllComponentTypes...> *) {
            if constexpr (std::is_same<T, void>()) {
                return 0;
            } else {
                uint32_t i = 0;
                uint32_t result = 0;
                bool found = (((i++, std::is_same<T, Tn>() ? (result = i, true) : false)) || ...) ||
                             (((i++, std::is_same<T, AllComponentTypes>() ? (result = i, true) : false)) || ...);
                Assertf(found, "Type missing from FieldTypes definition: %s", typeid(T).name());
                return result;
            }
        }

        template<typename... Tn,
            typename... AllComponentTypes,
            template<typename...> typename TupleType,
            template<typename...> typename ECSType>
        inline static uint32_t GetFieldTypeIndex(TupleType<Tn...> *,
            ECSType<AllComponentTypes...> *,
            const std::type_index &type) {
            if (type == typeid(void)) {
                return 0;
            } else {
                uint32_t i = 0;
                uint32_t result = 0;
                bool found = (((i++, type == typeid(Tn) ? (result = i, true) : false)) || ...) ||
                             (((i++, type == typeid(AllComponentTypes) ? (result = i, true) : false)) || ...);
                Assertf(found, "Type missing from FieldTypes definition: %s", type.name());
                return result;
            }
        }

        template<typename... Tn,
            typename... AllComponentTypes,
            template<typename...> typename TupleType,
            template<typename...> typename ECSType>
        inline static std::type_index GetFieldTypeIndex(TupleType<Tn...> *,
            ECSType<AllComponentTypes...> *,
            uint32_t typeIndex) {
            if (typeIndex == 0) {
                return typeid(void);
            } else {
                uint32_t i = 0;
                std::optional<std::type_index> result;
                bool found = (((typeIndex == ++i) ? (result = typeid(Tn), true) : false) || ...) ||
                             (((typeIndex == ++i) ? (result = typeid(AllComponentTypes), true) : false) || ...);
                Assertf(found, "Type missing from FieldTypes definition: %u", typeIndex);
                return *result;
            }
        }
    } // namespace detail

    template<typename Func>
    inline static auto GetComponentType(std::type_index type, Func &&func) {
        return detail::GetComponentType((ECS *)nullptr, type, std::forward<Func>(func));
    }

    template<typename Func>
    inline static auto GetFieldType(std::type_index type, Func &&func) {
        return detail::GetFieldType((FieldTypes *)nullptr, (ECS *)nullptr, type, std::forward<Func>(func));
    }

    template<typename Func>
    inline static auto GetFieldType(uint32_t typeIndex, Func &&func) {
        return detail::GetFieldType((FieldTypes *)nullptr, (ECS *)nullptr, typeIndex, std::forward<Func>(func));
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

    template<typename T>
    inline static uint32_t GetFieldTypeIndex() {
        return detail::GetFieldTypeIndex<T>((FieldTypes *)nullptr, (ECS *)nullptr);
    }
} // namespace ecs
