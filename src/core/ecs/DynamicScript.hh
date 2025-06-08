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

namespace ecs {
    struct DynamicScript final : public ScriptDefinitionBase, sp::NonMoveable {
        std::string name;
        std::optional<dynalo::library> dynamicLib;
        ScriptDefinition definition;
        const void *defaultContext = nullptr;
        void (*initFunc)(void *, ScriptState *) = nullptr;
        void (*onTickFunc)(void *, ScriptState *, void *, uint64_t, uint64_t) = nullptr;
        void (*onEventFunc)(void *, ScriptState *, void *, uint64_t, Event *) = nullptr;
        void (*prefabFunc)(const ScriptState *, void *, uint64_t, const sp::SceneRef *) = nullptr;

        const void *GetDefault() const override {
            return defaultContext;
        }

        void *AccessMut(ScriptState &state) const override {
            if (metadata.size == 0) return nullptr;
            void *ptr = std::any_cast<void *>(&state.scriptData);
            if (!ptr) ptr = &state.scriptData.emplace<void *>(std::malloc(metadata.size));
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<void *>(&state.scriptData);
            return ptr ? ptr : GetDefault();
        }

        static void Init(ScriptState &state) {
            void *ptr = std::any_cast<void *>(&state.scriptData);
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                if (!ptr) ptr = &state.scriptData.emplace<void *>(std::malloc(dynamicScript->metadata.size));
                if (dynamicScript->initFunc) dynamicScript->initFunc(ptr, &state);
            }
        }

        static void OnTick(ScriptState &state,
            const DynamicLock<ReadSignalsLock> &lock,
            Entity ent,
            chrono_clock::duration interval) {
            void *ptr = std::any_cast<void *>(&state.scriptData);
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                if (!ptr) ptr = &state.scriptData.emplace<void *>(std::malloc(dynamicScript->metadata.size));
                if (dynamicScript->onTickFunc) {
                    ecs::DynamicLock<> dynLock = lock;
                    dynamicScript->onTickFunc(ptr,
                        &state,
                        &dynLock,
                        (uint64_t)ent,
                        std::chrono::nanoseconds(interval).count());
                }
            }
        }

        static void OnEvent(ScriptState &state, const DynamicLock<ReadSignalsLock> &lock, Entity ent, Event event) {
            void *ptr = std::any_cast<void *>(&state.scriptData);
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                if (!ptr) ptr = &state.scriptData.emplace<void *>(std::malloc(dynamicScript->metadata.size));
                if (dynamicScript->onEventFunc) {
                    ecs::DynamicLock<> dynLock = lock;
                    dynamicScript->onEventFunc(ptr, &state, &dynLock, (uint64_t)ent, &event);
                }
            }
        }

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(state.definition.context)) {
                if (dynamicScript->prefabFunc) {
                    ecs::DynamicLock<> dynLock = lock;
                    dynamicScript->prefabFunc(&state, &dynLock, (uint64_t)ent, &scene);
                }
            }
        }

        // TODO: Stop leaking StructMetadata!
        DynamicScript(const std::string &name,
            dynalo::library &&lib,
            const ScriptDefinition &definition,
            const void *defaultContext,
            size_t contextSize,
            decltype(initFunc) initFunc,
            decltype(onTickFunc) onTickFunc)
            : ScriptDefinitionBase(
                  *new StructMetadata(typeid(void), contextSize, definition.name.c_str(), "DynamicScript")),
              name(name), dynamicLib(std::move(lib)), definition(definition), defaultContext(defaultContext),
              initFunc(initFunc), onTickFunc(onTickFunc) {
            this->definition.context = this;
            this->definition.initFunc = ScriptInitFunc(&Init);
            this->definition.callback = OnTickFunc(&OnTick);
        }

        // TODO: Stop leaking StructMetadata!
        DynamicScript(const std::string &name,
            dynalo::library &&lib,
            const ScriptDefinition &definition,
            const void *defaultContext,
            size_t contextSize,
            decltype(initFunc) initFunc,
            decltype(onEventFunc) onEventFunc)
            : ScriptDefinitionBase(*new StructMetadata(typeid(void), contextSize, name.c_str(), "DynamicScript")),
              name(name), dynamicLib(std::move(lib)), definition(definition), defaultContext(defaultContext),
              initFunc(initFunc), onEventFunc(onEventFunc) {
            this->definition.context = this;
            this->definition.initFunc = ScriptInitFunc(&Init);
            this->definition.callback = OnEventFunc(&OnEvent);
        }

        // TODO: Stop leaking StructMetadata!
        DynamicScript(const std::string &name,
            dynalo::library &&lib,
            const ScriptDefinition &definition,
            const void *defaultContext,
            size_t contextSize,
            decltype(prefabFunc) prefabFunc)
            : ScriptDefinitionBase(*new StructMetadata(typeid(void), contextSize, name.c_str(), "DynamicPrefab")),
              name(name), dynamicLib(std::move(lib)), definition(definition), defaultContext(defaultContext),
              prefabFunc(prefabFunc) {
            this->definition.context = this;
            this->definition.callback = PrefabFunc(&Prefab);
        }

        void Register() const {
            if (onTickFunc || onEventFunc) {
                GetScriptDefinitions().RegisterScript(ScriptDefinition(this->definition));
            }
            if (prefabFunc) {
                GetScriptDefinitions().RegisterPrefab(ScriptDefinition(this->definition));
            }
        }

        void Reload() {
            dynamicLib.reset();
            auto newScript = Load(name);
            if (!newScript) {
                Errorf("Failed to reload %s(%s)", name, definition.name);
                return;
            }
            dynamicLib = std::move(newScript->dynamicLib);
            definition = newScript->definition;
            definition.context = this;
            defaultContext = newScript->defaultContext;
            initFunc = newScript->initFunc;
            onTickFunc = newScript->onTickFunc;
            onEventFunc = newScript->onEventFunc;
            prefabFunc = newScript->prefabFunc;
        }

        static std::shared_ptr<DynamicScript> Load(const std::string &name) {
            dynalo::library dynamicLib(name);
            ScriptDefinition definition{};

            auto definitionFunc = dynamicLib.get_function<size_t(ScriptDefinition *)>("sp_script_get_definition");
            if (!definitionFunc) {
                Errorf("Failed to load %s, sp_script_get_definition() is missing", name);
                return nullptr;
            }
            auto getDefaultContextFunc = dynamicLib.get_function<const void *()>("sp_script_get_default_context");

            size_t contextSize = definitionFunc(&definition);
            const void *defaultContext = nullptr;
            if (getDefaultContextFunc && contextSize > 0) defaultContext = getDefaultContextFunc();
            if (!defaultContext) contextSize = 0;

            auto initFunc = dynamicLib.get_function<std::remove_pointer_t<decltype(DynamicScript::initFunc)>>(
                "sp_script_init");
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
                    Errorf("Failed to load %s(%s), sp_script_on_tick() is missing", name, definition.name);
                    return nullptr;
                }
                return std::make_shared<DynamicScript>(name,
                    std::move(dynamicLib),
                    definition,
                    defaultContext,
                    contextSize,
                    initFunc,
                    onTickFunc);
            case ScriptType::EventScript:
                if (!onEventFunc) {
                    Errorf("Failed to load %s(%s), sp_script_on_event() is missing", name, definition.name);
                    return nullptr;
                }
                return std::make_shared<DynamicScript>(name,
                    std::move(dynamicLib),
                    definition,
                    defaultContext,
                    contextSize,
                    initFunc,
                    onEventFunc);
            case ScriptType::PrefabScript:
                if (!prefabFunc) {
                    Errorf("Failed to load %s(%s), sp_script_prefab() is missing", name, definition.name);
                    return nullptr;
                }
                if (initFunc) Warnf("%s(%s) defines unsupported sp_script_init()", name, definition.name);
                return std::make_shared<DynamicScript>(name,
                    std::move(dynamicLib),
                    definition,
                    defaultContext,
                    contextSize,
                    prefabFunc);
            default:
                Errorf("DynamicScript %s(%s) unexpected script type: %s", name, definition.name, definition.type);
                return nullptr;
            }
        }
    };
} // namespace ecs
