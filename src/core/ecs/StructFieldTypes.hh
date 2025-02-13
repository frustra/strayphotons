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

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4554)
#endif

#include <hashes/fast_perfect_compressed_hash.hpp>

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

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
        std::string,

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
        glm::mat3,

        // Structs
        EntityRef,
        EventData,
        Transform,
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
        // TriggerGroup,
        TriggerShape,
        VisibilityMask,
        XrEye>;

    namespace detail {
        template<typename, size_t>
        struct PerfectHashLookup;

        template<typename... Tn, template<typename...> typename Wrapper, size_t LookupSize>
        struct PerfectHashLookup<Wrapper<Tn...>, LookupSize> {
            hashes::FastCompressedHash<size_t> perfectHash;
            using SizeType = std::conditional_t<sizeof...(Tn) >= 256, uint16_t, uint8_t>;
            std::array<SizeType, LookupSize> typeIndexes;

            constexpr PerfectHashLookup(hashes::FastCompressedHash<size_t> &&perfectHash) : perfectHash(perfectHash) {
                Assertf(perfectHash.modulo() <= LookupSize,
                    "PerfectHashLookup overflows allocated size: %llu > %llu",
                    perfectHash.modulo(),
                    LookupSize);
                typeIndexes.fill(std::numeric_limits<SizeType>::max());
                SizeType i = 0;
                (
                    [&] {
                        typeIndexes[perfectHash(typeid(Tn).hash_code())] = i++;
                    }(),
                    ...);
            }

            template<typename ResultT, typename Func>
            inline ResultT GetType(std::type_index type, Func &&func) const {
                static const std::array<ResultT (*)(Func &&), sizeof...(Tn)> callers = {[](Func &&fn) {
                    return std::invoke(fn, (Tn *)nullptr);
                }...};

                SizeType typeIndex = typeIndexes[perfectHash(type.hash_code())];
                return callers[typeIndex](std::forward<Func>(func));
            }
        };

        template<typename... Tn,
            template<typename...> typename Wrapper,
            typename... AllComponentTypes,
            template<typename...> typename ECSType>
        inline static auto CreateTypeLookup(Wrapper<Tn...> *, ECSType<AllComponentTypes...> *) {
            constexpr size_t typeCount = sizeof...(Tn) + sizeof...(AllComponentTypes);
            std::array<size_t, typeCount> typeHashes = {
                typeid(Tn).hash_code()...,
                typeid(AllComponentTypes).hash_code()...,
            };
            auto ph = hashes::create_fast_perfect_compressed_hash<size_t>(typeHashes.begin(), typeHashes.end());
            return PerfectHashLookup<Wrapper<Tn..., AllComponentTypes...>, typeCount * typeCount * 2>(std::move(ph));
        }

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

    static const auto &GetTypeLookup() {
        static const auto TypeLookup = detail::CreateTypeLookup((FieldTypes *)nullptr, (ECS *)nullptr);
        return TypeLookup;
    }

    template<typename ResultT, typename Func>
    inline static ResultT GetFieldType(std::type_index type, Func &&func) {
        return GetTypeLookup().GetType<ResultT>(type, std::forward<Func>(func));
    }

    template<typename ResultT, typename ArgType, typename Func>
    inline static ResultT GetFieldType(const std::type_index &type, ArgType *ptr, Func &&func) {
        return GetFieldType<ResultT>(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            if constexpr (std::is_const_v<ArgType>) {
                return func(*reinterpret_cast<const T *>(ptr));
            } else {
                return func(*reinterpret_cast<T *>(ptr));
            }
        });
    }
} // namespace ecs
