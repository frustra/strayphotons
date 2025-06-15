/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/ScriptImpl.hh"
#include "game/SceneRef.hh"

#include <dynalo/dynalo.hpp>
#include <memory>

namespace ecs {
    struct DynamicScript final : public ScriptDefinitionBase, sp::NonMoveable {
        std::string name;
        std::optional<dynalo::library> dynamicLib;
        StructMetadata metadata;
        ScriptDefinition definition;
        const void *defaultContext = nullptr;
        void (*initFunc)(void *, ScriptState *) = nullptr;
        void (*destroyFunc)(void *, ScriptState *) = nullptr;
        void (*onTickFunc)(void *, ScriptState *, void *, uint64_t, uint64_t) = nullptr;
        void (*onEventFunc)(void *, ScriptState *, void *, uint64_t, Event *) = nullptr;
        void (*prefabFunc)(const ScriptState *, void *, uint64_t, const sp::SceneRef *) = nullptr;

        std::shared_ptr<void> &MaybeAllocAccess(ScriptState &state) const {
            auto *ptr = std::any_cast<std::shared_ptr<void>>(&state.scriptData);
            if (!ptr) {
                std::shared_ptr<void> buffer(std::malloc(metadata.size), [](auto *ptr) {
                    std::free(ptr);
                });
                return state.scriptData.emplace<std::shared_ptr<void>>(std::move(buffer));
            }
            return *ptr;
        }

        const void *GetDefault() const override {
            return defaultContext;
        }

        void *AccessMut(ScriptState &state) const override {
            // TODO: Script shared_ptr state is not locked by Tecs
            if (metadata.size == 0) return nullptr;
            auto &ptr = MaybeAllocAccess(state);
            return ptr.get();
        }

        const void *Access(const ScriptState &state) const override {
            // TODO: Script shared_ptr state is not locked by Tecs
            const auto *ptr = std::any_cast<std::shared_ptr<void>>(&state.scriptData);
            return ptr ? ptr->get() : GetDefault();
        }

        static void Init(ScriptState &state) {
            ZoneScoped;
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                ZoneStr(dynamicScript->name);
                auto &ptr = dynamicScript->MaybeAllocAccess(state);
                if (dynamicScript->initFunc) dynamicScript->initFunc(ptr.get(), &state);
            }
        }

        static void Destroy(ScriptState &state) {
            ZoneScoped;
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                ZoneStr(dynamicScript->name);
                auto *ptr = std::any_cast<std::shared_ptr<void>>(&state.scriptData);
                if (ptr && dynamicScript->destroyFunc) dynamicScript->destroyFunc(ptr->get(), &state);
            }
        }

        static void OnTick(ScriptState &state,
            const DynamicLock<ReadSignalsLock> &lock,
            Entity ent,
            chrono_clock::duration interval) {
            ZoneScoped;
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                ZoneStr(dynamicScript->name);
                auto &ptr = dynamicScript->MaybeAllocAccess(state);
                if (dynamicScript->onTickFunc) {
                    ecs::DynamicLock<> dynLock = lock;
                    dynamicScript->onTickFunc(ptr.get(),
                        &state,
                        &dynLock,
                        (uint64_t)ent,
                        std::chrono::nanoseconds(interval).count());
                }
            }
        }

        static void OnEvent(ScriptState &state, const DynamicLock<ReadSignalsLock> &lock, Entity ent, Event event) {
            ZoneScoped;
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                ZoneStr(dynamicScript->name);
                auto &ptr = dynamicScript->MaybeAllocAccess(state);
                if (dynamicScript->onEventFunc) {
                    ecs::DynamicLock<> dynLock = lock;
                    dynamicScript->onEventFunc(ptr.get(), &state, &dynLock, (uint64_t)ent, &event);
                }
            }
        }

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            ZoneScoped;
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                ZoneStr(dynamicScript->name);
                if (dynamicScript->prefabFunc) {
                    ecs::DynamicLock<> dynLock = lock;
                    dynamicScript->prefabFunc(&state, &dynLock, (uint64_t)ent, &scene);
                }
            }
        }

        DynamicScript(const std::string &name,
            dynalo::library &&lib,
            const ScriptDefinition &definition,
            const void *defaultContext,
            size_t contextSize,
            decltype(initFunc) initFunc,
            decltype(destroyFunc) destroyFunc,
            decltype(onTickFunc) onTickFunc)
            : ScriptDefinitionBase(this->metadata), name(name), dynamicLib(std::move(lib)),
              metadata(typeid(void), contextSize, definition.name.c_str(), "DynamicScript"), definition(definition),
              defaultContext(defaultContext), initFunc(initFunc), destroyFunc(destroyFunc), onTickFunc(onTickFunc) {
            this->definition.context = this;
            this->definition.initFunc = ScriptInitFunc(&Init);
            this->definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            this->definition.callback = OnTickFunc(&OnTick);
        }

        DynamicScript(const std::string &name,
            dynalo::library &&lib,
            const ScriptDefinition &definition,
            const void *defaultContext,
            size_t contextSize,
            decltype(initFunc) initFunc,
            decltype(destroyFunc) destroyFunc,
            decltype(onEventFunc) onEventFunc)
            : ScriptDefinitionBase(this->metadata), name(name), dynamicLib(std::move(lib)),
              metadata(typeid(void), contextSize, name.c_str(), "DynamicScript"), definition(definition),
              defaultContext(defaultContext), initFunc(initFunc), destroyFunc(destroyFunc), onEventFunc(onEventFunc) {
            this->definition.context = this;
            this->definition.initFunc = ScriptInitFunc(&Init);
            this->definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            this->definition.callback = OnEventFunc(&OnEvent);
        }

        DynamicScript(const std::string &name,
            dynalo::library &&lib,
            const ScriptDefinition &definition,
            const void *defaultContext,
            size_t contextSize,
            decltype(prefabFunc) prefabFunc)
            : ScriptDefinitionBase(this->metadata), name(name), dynamicLib(std::move(lib)),
              metadata(typeid(void), contextSize, name.c_str(), "DynamicPrefab"), definition(definition),
              defaultContext(defaultContext), prefabFunc(prefabFunc) {
            this->definition.context = this;
            this->definition.callback = PrefabFunc(&Prefab);
        }

        void Register() const {
            GetScriptDefinitions().RegisterScript(ScriptDefinition(this->definition));
        }

        void Reload() {
            ZoneScoped;
            ZoneStr(name);
            dynamicLib.reset();
            auto newScript = Load(name);
            if (!newScript) {
                Errorf("Failed to reload %s(%s)", dynalo::to_native_name(name), definition.name);
                return;
            }
            dynamicLib = std::move(newScript->dynamicLib);
            definition = newScript->definition;
            definition.context = this;
            defaultContext = newScript->defaultContext;
            initFunc = newScript->initFunc;
            destroyFunc = newScript->destroyFunc;
            onTickFunc = newScript->onTickFunc;
            onEventFunc = newScript->onEventFunc;
            prefabFunc = newScript->prefabFunc;
        }

        static std::shared_ptr<DynamicScript> Load(const std::string &name) {
            ZoneScoped;
            ZoneStr(name);
            auto nativeName = dynalo::to_native_name(name);
            dynalo::library dynamicLib("./" + nativeName);
            ScriptDefinition definition{};

            auto definitionFunc = dynamicLib.get_function<size_t(ScriptDefinition *)>("sp_script_get_definition");
            if (!definitionFunc) {
                Errorf("Failed to load %s, sp_script_get_definition() is missing", nativeName);
                return nullptr;
            }
            auto getDefaultContextFunc = dynamicLib.get_function<const void *()>("sp_script_get_default_context");

            size_t contextSize = definitionFunc(&definition);
            const void *defaultContext = nullptr;
            if (getDefaultContextFunc && contextSize > 0) defaultContext = getDefaultContextFunc();
            if (!defaultContext) contextSize = 0;

            auto initFunc = dynamicLib.get_function<std::remove_pointer_t<decltype(DynamicScript::initFunc)>>(
                "sp_script_init");
            auto destroyFunc = dynamicLib.get_function<std::remove_pointer_t<decltype(DynamicScript::destroyFunc)>>(
                "sp_script_destroy");
            auto onTickFunc = dynamicLib.get_function<std::remove_pointer_t<decltype(DynamicScript::onTickFunc)>>(
                "sp_script_on_tick");
            auto onEventFunc = dynamicLib.get_function<std::remove_pointer_t<decltype(DynamicScript::onEventFunc)>>(
                "sp_script_on_event");
            auto prefabFunc = dynamicLib.get_function<std::remove_pointer_t<decltype(DynamicScript::prefabFunc)>>(
                "sp_script_prefab");

            switch (definition.type) {
            case ScriptType::LogicScript:
            case ScriptType::PhysicsScript:
                if (!onTickFunc) {
                    Errorf("Failed to load %s(%s), sp_script_on_tick() is missing", nativeName, definition.name);
                    return nullptr;
                }
                return std::make_shared<DynamicScript>(name,
                    std::move(dynamicLib),
                    definition,
                    defaultContext,
                    contextSize,
                    initFunc,
                    destroyFunc,
                    onTickFunc);
            case ScriptType::EventScript:
                if (!onEventFunc) {
                    Errorf("Failed to load %s(%s), sp_script_on_event() is missing", nativeName, definition.name);
                    return nullptr;
                }
                return std::make_shared<DynamicScript>(name,
                    std::move(dynamicLib),
                    definition,
                    defaultContext,
                    contextSize,
                    initFunc,
                    destroyFunc,
                    onEventFunc);
            case ScriptType::PrefabScript:
                if (!prefabFunc) {
                    Errorf("Failed to load %s(%s), sp_script_prefab() is missing", nativeName, definition.name);
                    return nullptr;
                }
                if (initFunc)
                    Warnf("%s(%s) PrefabScript defines unsupported sp_script_init()", nativeName, definition.name);
                if (destroyFunc)
                    Warnf("%s(%s) PrefabScript defines unsupported destroyFunc()", nativeName, definition.name);
                return std::make_shared<DynamicScript>(name,
                    std::move(dynamicLib),
                    definition,
                    defaultContext,
                    contextSize,
                    prefabFunc);
            default:
                Errorf("DynamicScript %s(%s) unexpected script type: %s", nativeName, definition.name, definition.type);
                return nullptr;
            }
        }
    };
} // namespace ecs
