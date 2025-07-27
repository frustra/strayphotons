/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Gui.hh"
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
        return ScriptDefinition{"", ScriptType::LogicScript, {}, false, {}, {}, {}, std::move(wrapperFn)};
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
        return ScriptDefinition{"", ScriptType::PhysicsScript, {}, false, {}, {}, {}, std::move(wrapperFn)};
    }
    template<typename... Events>
    static inline ScriptDefinition CreateEventScript(OnEventFunc &&callback, Events... events) {
        return ScriptDefinition{"", ScriptType::EventScript, {events...}, true, {}, {}, {}, callback};
    }
    static inline ScriptDefinition CreatePrefabScript(PrefabFunc &&callback) {
        return ScriptDefinition{"", ScriptType::PrefabScript, {}, false, {}, {}, {}, callback};
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

        static void Destroy(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) ptr->Destroy(state);
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
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::LogicScript, {}, false, savedPtr, ScriptInitFunc(&Init), {}, OnTickFunc(&OnTick)});
        }

        template<typename... Events>
        LogicScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::LogicScript,
                {events...},
                filterOnEvent,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
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

        static void Destroy(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) ptr->Destroy(state);
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
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::PhysicsScript, {}, false, savedPtr, ScriptInitFunc(&Init), {}, OnTickFunc(&OnTick)});
        }

        template<typename... Events>
        PhysicsScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::PhysicsScript,
                {events...},
                filterOnEvent,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
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

        static void Destroy(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) return;
            if constexpr (script_has_destroy_func<T>()) ptr->Destroy(state);
        }

        static void OnEvent(ScriptState &state, const DynamicLock<SendEventsLock> &lock, Entity ent, Event event) {
            T *ptr = std::any_cast<T>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<T>();
            ptr->OnEvent(state, lock, ent, event);
        }

        OnEventScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::EventScript, {}, true, savedPtr, ScriptInitFunc(&Init), {}, OnEventFunc(&OnEvent)});
        }

        template<typename... Events>
        OnEventScript(const std::string &name, const StructMetadata &metadata, Events... events)
            : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::EventScript,
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
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript(
                {name, ScriptType::PrefabScript, {}, false, savedPtr, {}, {}, PrefabFunc(&Prefab)});
        }
    };

    // Checks if the script has an Init(ScriptState &state, GuiRenderable &gui) function
    template<typename T, typename = void>
    struct script_has_gui_init_func : std::false_type {};
    template<typename T>
    struct script_has_gui_init_func<T,
        std::void_t<decltype(std::declval<T>().Init(std::declval<ScriptState &>(), std::declval<GuiRenderable &>()))>>
        : std::true_type {};

    // Checks if the script has a PreDefine(), DefineContents(), or PostDefine() function
    template<typename T, typename = void>
    struct script_has_predefine_func : std::false_type {};
    template<typename T>
    struct script_has_predefine_func<T,
        std::void_t<decltype(std::declval<T>().PreDefine(std::declval<ScriptState &>(), std::declval<Entity>()))>>
        : std::true_type {};
    template<typename T, typename = void>
    struct script_has_definecontents_func : std::false_type {};
    template<typename T>
    struct script_has_definecontents_func<T,
        std::void_t<decltype(std::declval<T>().DefineContents(std::declval<ScriptState &>(), std::declval<Entity>()))>>
        : std::true_type {};
    template<typename T, typename = void>
    struct script_has_postdefine_func : std::false_type {};
    template<typename T>
    struct script_has_postdefine_func<T,
        std::void_t<decltype(std::declval<T>().PostDefine(std::declval<ScriptState &>(), std::declval<Entity>()))>>
        : std::true_type {};

    template<typename T>
    class ScriptGuiRenderable final : public GuiRenderable {
    public:
        ScriptGuiRenderable(std::string_view name)
            : GuiRenderable(name, GuiLayoutAnchor::Floating), scriptData({}), state(nullptr) {}

        T scriptData;

        void Init(ScriptState &state) {
            this->state = &state;
            if constexpr (script_has_init_func<T>()) scriptData.Init(state);
            if constexpr (script_has_gui_init_func<T>()) scriptData.Init(state, *(GuiRenderable *)this);
            savedPtr = std::shared_ptr<ScriptGuiRenderable<T>>(this, [](auto *) {});
        }

        void Destroy(ScriptState &state) {
            Assertf(savedPtr.use_count() == 1,
                "Destroying ScriptGuiRenderable that's in use: %s",
                state.definition.name);
            savedPtr.reset();
            if constexpr (script_has_destroy_func<T>()) scriptData.Destroy(state);
            this->state = nullptr;
        }

        bool PreDefine(Entity ent) override {
            if (!state) return false;
            if constexpr (script_has_predefine_func<T>()) {
                return scriptData.PreDefine(*state, ent);
            } else {
                return true;
            }
        }

        void DefineContents(Entity ent) override {
            Assertf(state != nullptr, "ScriptGuiRenderable::DefineContents called without state init");
            scriptData.DefineContents(*state, ent);
        }

        void PostDefine(Entity ent) override {
            Assertf(state != nullptr, "ScriptGuiRenderable::PostDefine called without state init");
            if constexpr (script_has_postdefine_func<T>()) scriptData.PostDefine(*state, ent);
        }

        std::shared_ptr<GuiRenderable> GetGuiRenderable(ScriptState &state) {
            Assertf(savedPtr, "ScriptGuiRenderable::GetGuiRenderable called without state init");
            return savedPtr;
        }

    private:
        ScriptState *state = nullptr;
        std::shared_ptr<ScriptGuiRenderable<T>> savedPtr;
    };

    template<typename T>
    struct GuiScript final : public ScriptDefinitionBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *AccessMut(ScriptState &state) const override {
            auto *ptr = std::any_cast<ScriptGuiRenderable<T>>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<ScriptGuiRenderable<T>>(state.definition.name);
            return &ptr->scriptData;
        }

        const void *Access(const ScriptState &state) const override {
            const auto *ptr = std::any_cast<ScriptGuiRenderable<T>>(&state.scriptData);
            return ptr ? &ptr->scriptData : &defaultValue;
        }

        static void Init(ScriptState &state) {
            auto *ptr = std::any_cast<ScriptGuiRenderable<T>>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<ScriptGuiRenderable<T>>(state.definition.name);
            ptr->Init(state);
        }

        static void Destroy(ScriptState &state) {
            auto *ptr = std::any_cast<ScriptGuiRenderable<T>>(&state.scriptData);
            if (ptr) ptr->Destroy(state);
        }

        static std::shared_ptr<GuiRenderable> GetGuiRenderable(ScriptState &state) {
            auto *ptr = std::any_cast<ScriptGuiRenderable<T>>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<ScriptGuiRenderable<T>>(state.definition.name);
            return ptr->GetGuiRenderable(state);
        }

        GuiScript(const std::string &name, const StructMetadata &metadata) : ScriptDefinitionBase(metadata) {
            static const std::shared_ptr<ScriptDefinitionBase> savedPtr(this, [](auto *) {});
            GetScriptDefinitions().RegisterScript({name,
                ScriptType::GuiScript,
                {},
                false,
                savedPtr,
                ScriptInitFunc(&Init),
                ScriptDestroyFunc(&Destroy),
                GuiRenderableFunc(&GetGuiRenderable)});
        }
    };
} // namespace ecs
