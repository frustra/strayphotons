/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Scripts.hh"

namespace ecs {
    template<typename LockType>
    static inline ScriptDefinition CreateLogicScript(
        std::function<void(ScriptState &, LockType, Entity, chrono_clock::duration)> &&callback) {
        auto wrapperFn = [callback = std::move(callback)](ecs::ScriptState &state,
                             ecs::DynamicLock<ecs::ReadSignalsLock> lock,
                             ecs::Entity ent,
                             chrono_clock::duration interval) {
            auto tryLock = lock.TryLock<LockType>();
            Assertf(tryLock, "LogicScript invoked without lock permissions: %s", typeid(LockType).name());
            return callback(state, *tryLock, ent, interval);
        };
        return ScriptDefinition{"", ScriptType::LogicScript, {}, false, nullptr, {}, {}, std::move(wrapperFn)};
    }
    template<typename LockType>
    static inline ScriptDefinition CreatePhysicsScript(
        std::function<void(ScriptState &, LockType, Entity, chrono_clock::duration)> &&callback) {
        auto wrapperFn = [callback = std::move(callback)](ecs::ScriptState &state,
                             ecs::DynamicLock<ecs::ReadSignalsLock> lock,
                             ecs::Entity ent,
                             chrono_clock::duration interval) {
            auto tryLock = lock.TryLock<LockType>();
            Assertf(tryLock, "PhysicsScript invoked without lock permissions: %s", typeid(LockType).name());
            return callback(state, *tryLock, ent, interval);
        };
        return ScriptDefinition{"", ScriptType::PhysicsScript, {}, false, nullptr, {}, {}, std::move(wrapperFn)};
    }
    template<typename... Events>
    static inline ScriptDefinition CreateEventScript(OnEventFunc &&callback, Events... events) {
        return ScriptDefinition{"", ScriptType::EventScript, {events...}, true, nullptr, {}, {}, callback};
    }
    static inline ScriptDefinition CreatePrefabScript(PrefabFunc &&callback) {
        return ScriptDefinition{"", ScriptType::PrefabScript, {}, false, nullptr, {}, {}, callback};
    }

    // Checks if the script has an Init(ScriptState &state) function
    template<typename T, typename = void>
    struct script_has_init_func : std::false_type {};
    template<typename T>
    struct script_has_init_func<T, std::void_t<decltype(std::declval<T>().Init(std::declval<ScriptState &>()))>>
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
            void *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.scriptData);
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnTick(ScriptState &state,
            const DynamicLock<ReadSignalsLock> &lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            using LockType = script_ontick_lock_t<T>::LockType;
            auto tryLock = lock.TryLock<LockType>();
            Assertf(tryLock, "Failed to lock ontick script lock: %s", typeid(LockType).name());
            ptr->OnTick(state, *tryLock, ent, interval);
        }

        LogicScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::LogicScript, {}, false, this, ScriptInitFunc(&Init), {}, OnTickFunc(&OnTick)});
        }

        template<typename... Events>
        LogicScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::LogicScript,
                {events...},
                filterOnEvent,
                this,
                ScriptInitFunc(&Init),
                {},
                OnTickFunc(&OnTick)});
        }
    };

    template<typename T>
    struct PhysicsScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *AccessMut(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.scriptData);
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnTick(ScriptState &state,
            const DynamicLock<ReadSignalsLock> &lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            using LockType = script_ontick_lock_t<T>::LockType;
            auto tryLock = lock.TryLock<LockType>();
            Assertf(tryLock, "Failed to lock ontick physics script lock: %s", typeid(LockType).name());
            ptr->OnTick(state, *tryLock, ent, interval);
        }

        PhysicsScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::PhysicsScript, {}, false, this, ScriptInitFunc(&Init), {}, OnTickFunc(&OnTick)});
        }

        template<typename... Events>
        PhysicsScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::PhysicsScript,
                {events...},
                filterOnEvent,
                this,
                ScriptInitFunc(&Init),
                {},
                OnTickFunc(&OnTick)});
        }
    };

    template<typename T>
    struct OnEventScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *AccessMut(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.scriptData);
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnEvent(ScriptState &state, const DynamicLock<SendEventsLock> &lock, Entity ent, Event event) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            ptr->OnEvent(state, lock, ent, event);
        }

        OnEventScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::EventScript, {}, true, this, ScriptInitFunc(&Init), {}, OnEventFunc(&OnEvent)});
        }

        template<typename... Events>
        OnEventScript(const std::string &name, const StructMetadata &metadata, Events... events)
            : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::EventScript,
                {events...},
                true,
                this,
                ScriptInitFunc(&Init),
                {},
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
            void *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.scriptData);
            return ptr ? ptr : &defaultValue;
        }

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            const T *ptr = std::any_cast<T>(&state.scriptData);
            T data;
            if (ptr) data = *ptr;
            data.Prefab(state, scene.Lock(), lock, ent);
        }

        PrefabScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::PrefabScript, {}, false, this, {}, {}, PrefabFunc(&Prefab)});
        }
    };
} // namespace ecs
