/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/ScriptDefinition.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/components/RenderOutput.hh"

#include <utility>

namespace sp {
    class GenericCompositor;
}

namespace ecs {
    static inline ScriptDefinition CreateLogicScript(LogicTickFunc &&callback) {
        return ScriptDefinition{"",
            ScriptType::LogicScript,
            LogicUpdateLock::GetReadPermissions(),
            LogicUpdateLock::GetWritePermissions(),
            {},
            false,
            {},
            {},
            {},
            callback};
    }
    static inline ScriptDefinition CreatePhysicsScript(PhysicsTickFunc &&callback) {
        return ScriptDefinition{"",
            ScriptType::PhysicsScript,
            PhysicsUpdateLock::GetReadPermissions(),
            PhysicsUpdateLock::GetWritePermissions(),
            {},
            false,
            {},
            {},
            {},
            callback};
    }
    template<typename... Events>
    static inline ScriptDefinition CreateEventScript(PermissionBitset readPermissions,
        PermissionBitset writePermissions,
        OnEventFunc &&callback,
        Events... events) {
        return ScriptDefinition{"",
            ScriptType::EventScript,
            readPermissions,
            writePermissions,
            {events...},
            true,
            {},
            {},
            {},
            callback};
    }
    static inline ScriptDefinition CreatePrefabScript(PrefabFunc &&callback) {
        return ScriptDefinition{"",
            ScriptType::PrefabScript,
            Lock<AddRemove>::GetReadPermissions(),
            Lock<AddRemove>::GetWritePermissions(),
            {},
            false,
            {},
            {},
            {},
            callback};
    }

    // Checks if the script has an Init(ScriptState &state) function
    template<typename T, typename = void>
    struct script_has_init_func : std::false_type {};
    template<typename T>
    struct script_has_init_func<T, std::void_t<decltype(std::declval<T>().Init(std::declval<ScriptState &>()))>>
        : std::true_type {};

    // Checks if the script has an Destroy(ScriptState &state) function
    template<typename T, typename = void>
    struct script_has_destroy_func : std::false_type {};
    template<typename T>
    struct script_has_destroy_func<T, std::void_t<decltype(std::declval<T>().Destroy(std::declval<ScriptState &>()))>>
        : std::true_type {};

    template<typename T>
    struct script_ontick_lock_t {
        template<typename LockType>
        static inline consteval LockType *ptrLookup(
            void (T::*F)(ScriptState &, LockType, Entity, chrono_clock::duration)) {
            return nullptr;
        }

        using LockType = std::remove_pointer_t<decltype(ptrLookup(&T::OnTick))>;
    };

    template<typename T>
    struct LogicScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

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
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void Destroy(ScriptState &state) {
            T *ptr = state.Get<T>();
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) ptr->Destroy(state);
        }

        static void OnTick(ScriptState &state,
            const LogicUpdateLock &lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            using LockType = script_ontick_lock_t<T>::LockType;
            ptr->OnTick(state, (LockType)lock, ent, interval);
        }

        LogicScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::LogicScript,
                LogicUpdateLock::GetReadPermissions(),
                LogicUpdateLock::GetWritePermissions(),
                {},
                false,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                LogicTickFunc(&OnTick)});
        }

        template<typename... Events>
        LogicScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::LogicScript,
                LogicUpdateLock::GetReadPermissions(),
                LogicUpdateLock::GetWritePermissions(),
                {events...},
                filterOnEvent,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                LogicTickFunc(&OnTick)});
        }
    };

    template<typename T>
    struct PhysicsScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

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
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void Destroy(ScriptState &state) {
            T *ptr = state.Get<T>();
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) ptr->Destroy(state);
        }

        static void OnTick(ScriptState &state,
            const PhysicsUpdateLock &lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            using LockType = script_ontick_lock_t<T>::LockType;
            ptr->OnTick(state, (LockType)lock, ent, interval);
        }

        PhysicsScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::PhysicsScript,
                PhysicsUpdateLock::GetReadPermissions(),
                PhysicsUpdateLock::GetWritePermissions(),
                {},
                false,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                PhysicsTickFunc(&OnTick)});
        }

        template<typename... Events>
        PhysicsScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::PhysicsScript,
                PhysicsUpdateLock::GetReadPermissions(),
                PhysicsUpdateLock::GetWritePermissions(),
                {events...},
                filterOnEvent,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                PhysicsTickFunc(&OnTick)});
        }
    };

    template<typename T>
    struct script_onevent_lock_t {
        template<typename LockType>
        static inline consteval LockType *ptrLookup(void (T::*F)(ScriptState &, LockType, Entity, Event)) {
            return nullptr;
        }

        using LockType = std::remove_pointer_t<decltype(ptrLookup(&T::OnEvent))>;
    };

    template<typename T>
    struct EventScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

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
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void Destroy(ScriptState &state) {
            T *ptr = state.Get<T>();
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) ptr->Destroy(state);
        }

        static void OnEvent(ScriptState &state, const DynamicLock<SendEventsLock> &lock, Entity ent, Event event) {
            T *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            using LockType = script_onevent_lock_t<T>::LockType;
            auto dynamicLock = lock.TryLock<LockType>();
            Assertf(dynamicLock.has_value(), "OnEvent script failed to obtain dynamic lock");
            ptr->OnEvent(state, (LockType)dynamicLock.value(), ent, event);
        }

        template<typename... Events>
        EventScript(const std::string &name, const StructMetadata &metadata, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            using LockType = script_onevent_lock_t<T>::LockType;
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::EventScript,
                LockType::GetReadPermissions(),
                LockType::GetWritePermissions(),
                {events...},
                true,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                OnEventFunc(&OnEvent)});
        }
    };

    template<typename T>
    struct PrefabScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

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

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            const T *ptr = state.Get<T>();
            T data;
            if (ptr) data = *ptr;
            data.Prefab(state, scene.Lock(), lock, ent);
        }

        PrefabScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::PrefabScript,
                Lock<AddRemove>::GetReadPermissions(),
                Lock<AddRemove>::GetWritePermissions(),
                {},
                false,
                savedPtr,
                {},
                {},
                PrefabFunc(&Prefab)});
        }
    };

    // Checks if the script has an BeforeFrame() function
    template<typename T, typename = void>
    struct script_has_before_frame_func : std::false_type {};
    template<typename T>
    struct script_has_before_frame_func<T,
        std::void_t<decltype(std::declval<T>().BeforeFrame(std::declval<sp::GenericCompositor &>(),
            std::declval<ScriptState &>(),
            std::declval<Entity>()))>> : std::true_type {};

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
    struct GuiScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *AccessMut(ScriptState &state) const override {
            auto *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const auto *ptr = state.Get<T>();
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            auto *ptr = state.Get<T>();
            if (!ptr) ptr = state.Set<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void Destroy([[maybe_unused]] ScriptState &state) {
            if constexpr (script_has_destroy_func<T>()) {
                auto *ptr = state.Get<T>();
                if (ptr) ptr->Destroy(state);
            }
        }

        static bool BeforeFrame([[maybe_unused]] sp::GenericCompositor &compositor,
            [[maybe_unused]] ScriptState &state,
            [[maybe_unused]] Entity ent) {
            if constexpr (script_has_before_frame_func<T>()) {
                auto *ptr = state.Get<T>();
                if (!ptr) ptr = state.Set<T>();
                return ptr->BeforeFrame(compositor, state, ent);
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
            if constexpr (script_has_render_gui_func<T>()) {
                auto *ptr = state.Get<T>();
                if (!ptr) ptr = state.Set<T>();
                ptr->RenderGui(compositor, state, ent, displaySize, scale, deltaTime, result);
            }
        }

        GuiScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::GuiScript,
                PermissionBitset(),
                PermissionBitset(),
                {},
                false,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                GuiRenderFuncs(&BeforeFrame, &RenderGui)});
        }
    };
} // namespace ecs
