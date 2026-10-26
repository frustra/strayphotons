/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "ecs/ScriptDefinition.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/components/RenderOutput.hh"

#include <utility>

namespace sp {
    class GenericCompositor;
}

namespace ecs {
    static inline ScriptDefinition CreateLogicScript(LogicTickFunc &&callback) {
        return ScriptDefinition{
            .logicTickFunc =
                {
                    callback,
                    LogicUpdateLock::GetReadPermissions(),
                    LogicUpdateLock::GetWritePermissions(),
                },
        };
    }
    static inline ScriptDefinition CreatePhysicsScript(PhysicsTickFunc &&callback) {
        return ScriptDefinition{
            .physicsTickFunc =
                {
                    callback,
                    PhysicsUpdateLock::GetReadPermissions(),
                    PhysicsUpdateLock::GetWritePermissions(),
                },
        };
    }
    template<typename... Events>
    static inline ScriptDefinition CreateEventScript(PermissionBitset readPermissions,
        PermissionBitset writePermissions,
        OnEventFunc &&callback,
        Events... events) {
        return ScriptDefinition{
            .events = {events...},
            .onEventFunc = {callback, readPermissions, writePermissions},
        };
    };

    static inline ScriptDefinition CreatePrefabScript(PrefabFunc &&callback) {
        return ScriptDefinition{
            .prefabFunc = callback,
        };
    }

    // Checks if the script has an Init(ScriptState &) function
    template<typename T, typename = void>
    struct script_has_init_func : std::false_type {};
    template<typename T>
    struct script_has_init_func<T, std::void_t<decltype(std::declval<T>().Init(std::declval<ScriptState &>()))>>
        : std::true_type {};

    // Checks if the script has an Destroy(ScriptState &) function
    template<typename T, typename = void>
    struct script_has_destroy_func : std::false_type {};
    template<typename T>
    struct script_has_destroy_func<T, std::void_t<decltype(std::declval<T>().Destroy(std::declval<ScriptState &>()))>>
        : std::true_type {};

    // Checks if the script has an OnLogicTick(...) function
    template<typename T, typename = void>
    struct script_has_logic_tick_func : std::false_type {};
    template<typename T>
    struct script_has_logic_tick_func<T,
        std::void_t<decltype(std::declval<T>().OnLogicTick(std::declval<ScriptState &>(),
            std::declval<const LogicUpdateLock &>(),
            std::declval<Entity>(),
            std::declval<chrono_clock::duration>()))>> : std::true_type {};

    template<typename T>
    struct script_logic_tick_lock_t {
        template<typename LockType>
        static inline consteval LockType *ptrLookup(
            void (T::*F)(ScriptState &, LockType, Entity, chrono_clock::duration)) {
            return nullptr;
        }

        using LockType = std::remove_pointer_t<decltype(ptrLookup(&T::OnLogicTick))>;
    };

    // Checks if the script has an OnPhysicsTick(...) function
    template<typename T, typename = void>
    struct script_has_physics_tick_func : std::false_type {};
    template<typename T>
    struct script_has_physics_tick_func<T,
        std::void_t<decltype(std::declval<T>().OnPhysicsTick(std::declval<ScriptState &>(),
            std::declval<const PhysicsUpdateLock &>(),
            std::declval<Entity>(),
            std::declval<chrono_clock::duration>()))>> : std::true_type {};

    template<typename T>
    struct script_physics_tick_lock_t {
        template<typename LockType>
        static inline consteval LockType *ptrLookup(
            void (T::*F)(ScriptState &, LockType, Entity, chrono_clock::duration)) {
            return nullptr;
        }

        using LockType = std::remove_pointer_t<decltype(ptrLookup(&T::OnPhysicsTick))>;
    };

    // Checks if the script has an OnEvent(...) function
    template<typename T, typename = void>
    struct script_has_onevent_func : std::false_type {};
    template<typename T>
    struct script_has_onevent_func<T,
        std::void_t<decltype(std::declval<T>().OnEvent(std::declval<ScriptState &>(),
            std::declval<const Lock<AddRemove> &>(),
            std::declval<Entity>(),
            std::declval<Event>()))>> : std::true_type {};

    template<typename T>
    struct script_onevent_lock_t {
        template<typename LockType>
        static inline consteval LockType *ptrLookup(void (T::*F)(ScriptState &, LockType, Entity, Event)) {
            return nullptr;
        }

        using LockType = std::remove_pointer_t<decltype(ptrLookup(&T::OnEvent))>;
    };

    // Checks if the script has a Prefab(...) function
    template<typename T, typename = void>
    struct script_has_prefab_func : std::false_type {};
    template<typename T>
    struct script_has_prefab_func<T,
        std::void_t<decltype(std::declval<T>().Prefab(std::declval<const ScriptState &>(),
            std::declval<const std::shared_ptr<sp::Scene> &>(),
            std::declval<Lock<AddRemove>>(),
            std::declval<Entity>()))>> : std::true_type {};

    // Checks if the script has an BeforeFrame() function
    template<typename T, typename = void>
    struct script_has_before_frame_func : std::false_type {};
    template<typename T>
    struct script_has_before_frame_func<T,
        std::void_t<decltype(std::declval<T>().BeforeFrame(std::declval<sp::GenericCompositor &>(),
                                 std::declval<ScriptState &>(),
                                 std::declval<Entity>()),
            std::declval<const sp::HeapVector<sp::GuiInputEvent> &>())>> : std::true_type {};

    // Checks if the script has a RenderGui() function
    template<typename T, typename = void>
    struct script_has_render_gui_func : std::false_type {};
    template<typename T>
    struct script_has_render_gui_func<T,
        std::void_t<decltype(std::declval<T>().RenderGui(std::declval<sp::GenericCompositor &>(),
            std::declval<ScriptState &>(),
            std::declval<Entity>(),
            std::declval<glm::vec2>(),
            std::declval<glm::vec2>(),
            std::declval<float>(),
            std::declval<sp::GuiDrawData &>()))>> : std::true_type {};

    template<typename T>
    struct BuiltInScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

        static_assert(script_has_logic_tick_func<T>() || script_has_physics_tick_func<T>() ||
                          script_has_prefab_func<T>() || script_has_onevent_func<T>() ||
                          script_has_before_frame_func<T>() || script_has_render_gui_func<T>(),
            "BuiltInScript is missing function definitions");

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *AccessMut(ScriptState &state) const override {
            void *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = state.Get<T>();
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            DebugZoneScoped;
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void Destroy(ScriptState &state) {
            DebugZoneScoped;
            T *ptr = state.Get<T>();
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) {
                ptr->Destroy(state);
            }
            state.Reset();
        }

        static void OnLogicTick(ScriptState &state,
            const LogicUpdateLock &lock,
            Entity ent,
            chrono_clock::duration interval) {
            DebugZoneScoped;
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            using LockType = script_logic_tick_lock_t<T>::LockType;
            ptr->OnLogicTick(state, (LockType)lock, ent, interval);
        }

        static void OnPhysicsTick(ScriptState &state,
            const PhysicsUpdateLock &lock,
            Entity ent,
            chrono_clock::duration interval) {
            DebugZoneScoped;
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            using LockType = script_physics_tick_lock_t<T>::LockType;
            ptr->OnPhysicsTick(state, (LockType)lock, ent, interval);
        }

        static void OnEvent(ScriptState &state, const DynamicLock<SendEventsLock> &lock, Entity ent, Event event) {
            DebugZoneScoped;
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            using LockType = script_onevent_lock_t<T>::LockType;
            auto dynamicLock = lock.TryLock<LockType>();
            Assertf(dynamicLock.has_value(), "OnEvent script failed to obtain dynamic lock");
            ptr->OnEvent(state, (LockType)dynamicLock.value(), ent, event);
        }

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            DebugZoneScoped;
            const T *ptr = state.Get<T>();
            T data;
            if (ptr) data = *ptr;
            data.Prefab(state, scene.Lock(), lock, ent);
        }

        static bool BeforeFrame([[maybe_unused]] sp::GenericCompositor &compositor,
            [[maybe_unused]] ScriptState &state,
            [[maybe_unused]] Entity ent,
            [[maybe_unused]] const sp::HeapVector<sp::GuiInputEvent> &inputEvents) {
            DebugZoneScoped;
            if constexpr (script_has_before_frame_func<T>()) {
                auto *ptr = state.Get<T>();
                if (!ptr) ptr = state.Set<T>();
                return ptr->BeforeFrame(compositor, state, ent, inputEvents);
            } else {
                Warnf("GuiScript %s has no BeforeFrame function", state.definition.name);
                return false;
            }
        }

        static void RenderGui([[maybe_unused]] sp::GenericCompositor &compositor,
            [[maybe_unused]] ScriptState &state,
            [[maybe_unused]] Entity ent,
            [[maybe_unused]] glm::vec2 displaySize,
            [[maybe_unused]] glm::vec2 scale,
            [[maybe_unused]] float deltaTime,
            [[maybe_unused]] sp::GuiDrawData &result) {
            DebugZoneScoped;
            if constexpr (script_has_render_gui_func<T>()) {
                auto *ptr = state.Get<T>();
                if (!ptr) ptr = state.Set<T>();
                ptr->RenderGui(compositor, state, ent, displaySize, scale, deltaTime, result);
            }
        }

        template<typename... Events>
        BuiltInScript(const std::string &name, const StructMetadata &metadata, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            ScriptDefinition definition{
                .name = name,
                .events = {events...},
                .context = savedPtr,
                .initFunc = ScriptInitFunc(&Init),
                .destroyFunc = ScriptDestroyFunc(&Destroy),
            };
            if constexpr (script_has_logic_tick_func<T>()) {
                using LockType = script_logic_tick_lock_t<T>::LockType;
                definition.logicTickFunc = {
                    LogicTickFunc(&OnLogicTick),
                    LockType::GetReadPermissions(),
                    LockType::GetWritePermissions(),
                };
            }
            if constexpr (script_has_physics_tick_func<T>()) {
                using LockType = script_physics_tick_lock_t<T>::LockType;
                definition.physicsTickFunc = {
                    PhysicsTickFunc(&OnPhysicsTick),
                    LockType::GetReadPermissions(),
                    LockType::GetWritePermissions(),
                };
            }
            if constexpr (script_has_onevent_func<T>()) {
                using LockType = script_onevent_lock_t<T>::LockType;
                definition.onEventFunc = {
                    OnEventFunc(&OnEvent),
                    LockType::GetReadPermissions(),
                    LockType::GetWritePermissions(),
                };
            }
            if constexpr (script_has_prefab_func<T>()) {
                definition.prefabFunc = PrefabFunc(&Prefab);
            }
            if constexpr (script_has_before_frame_func<T>()) {
                definition.beforeFrameFunc = BeforeFrameFunc(&BeforeFrame);
            }
            if constexpr (script_has_render_gui_func<T>()) {
                definition.renderGuiFunc = RenderGuiFunc(&RenderGui);
            }
            GetScriptDefinitions().RegisterScript(std::move(definition));
        }
    };
} // namespace ecs
