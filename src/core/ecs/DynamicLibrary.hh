/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/ScriptImpl.hh"
#include "game/SceneRef.hh"

#include <memory>

namespace dynalo {
    class library;
}

namespace ecs {
    class DynamicScript;
    class ScriptManager;

    class DynamicLibrary : public sp::NonMoveable {
    public:
        const std::string name;

        void RegisterScripts() const;
        void ReloadLibrary();
        std::vector<std::shared_ptr<DynamicScript>> &GetScripts();

        static std::shared_ptr<DynamicLibrary> Load(const std::string &name);

    private:
        DynamicLibrary(const std::string &name, dynalo::library &&lib);

        std::shared_ptr<dynalo::library> dynamicLib;
        std::vector<std::shared_ptr<DynamicScript>> scripts;

        friend class ScriptManager;
    };

    class DynamicScriptContext {
    public:
        DynamicScriptContext() : context(nullptr), script(nullptr) {}
        DynamicScriptContext(const std::shared_ptr<DynamicScript> &script);
        DynamicScriptContext(const DynamicScriptContext &other);
        DynamicScriptContext(DynamicScriptContext &&other) = default;
        ~DynamicScriptContext();

        DynamicScriptContext &operator=(const DynamicScriptContext &other);
        DynamicScriptContext &operator=(DynamicScriptContext &&other) = default;

        void *context = nullptr;

    private:
        std::shared_ptr<DynamicScript> script = nullptr;
    };

    struct DynamicScriptDefinition {
        std::string name;
        ScriptType type;
        std::vector<std::string> events;
        bool filterOnEvent = false;

        void *(*newContextFunc)(const void *) = nullptr;
        void (*freeContextFunc)(void *) = nullptr;
        void (*initFunc)(void *, ScriptState *) = nullptr;
        void (*destroyFunc)(void *, ScriptState *) = nullptr;
        void (*onTickFunc)(void *, ScriptState *, DynamicLock<> *, Entity, uint64_t) = nullptr;
        void (*onEventFunc)(void *, ScriptState *, DynamicLock<> *, Entity, Event *) = nullptr;
        void (*prefabFunc)(const ScriptState *, DynamicLock<> *, Entity, const sp::SceneRef *) = nullptr;
    };

    static StructMetadata MetadataDynamicScriptDefinition(typeid(DynamicScriptDefinition),
        sizeof(DynamicScriptDefinition),
        "DynamicScriptDefinition",
        "A definition describing the name, type, and functions of a script",
        StructField::New("name", "The name of the script", &DynamicScriptDefinition::name),
        StructField::New("type", "The type of the script", &DynamicScriptDefinition::type),
        StructField::New("events",
            "A list of the names of events this script can receive",
            &DynamicScriptDefinition::events),
        StructField::New("filter_on_event",
            "True if this script should only run if new events are received",
            &DynamicScriptDefinition::filterOnEvent),
        StructField::New("new_context_func", &DynamicScriptDefinition::newContextFunc),
        StructField::New("free_context_func", &DynamicScriptDefinition::freeContextFunc),
        StructField::New("init_func", &DynamicScriptDefinition::initFunc),
        StructField::New("destroy_func", &DynamicScriptDefinition::destroyFunc),
        StructField::New("on_tick_func", &DynamicScriptDefinition::onTickFunc),
        StructField::New("on_event_func", &DynamicScriptDefinition::onEventFunc),
        StructField::New("prefab_func", &DynamicScriptDefinition::prefabFunc));

    class DynamicScript final : public ScriptDefinitionBase, sp::NonMoveable {
    public:
        StructMetadata metadata;
        ScriptDefinition definition;
        DynamicLibrary *library;

        void Register() const;

        const void *GetDefault() const override;
        const void *Access(const ScriptState &state) const override;
        void *AccessMut(ScriptState &state) const override;

    private:
        DynamicScript(DynamicLibrary &library, const DynamicScriptDefinition &dynamicDefinition);
        static std::shared_ptr<DynamicScript> Load(DynamicLibrary &library, const DynamicScriptDefinition &definition);

        DynamicScriptDefinition dynamicDefinition;
        DynamicScriptContext defaultContext;

        DynamicScriptContext &MaybeAllocContext(ScriptState &state) const;

        static void Init(ScriptState &state);
        static void Destroy(ScriptState &state);

        static void OnTick(ScriptState &state,
            const DynamicLock<ReadSignalsLock> &lock,
            Entity ent,
            chrono_clock::duration interval);
        static void OnEvent(ScriptState &state, const DynamicLock<ReadSignalsLock> &lock, Entity ent, Event event);
        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent);

        friend class DynamicScriptContext;
        friend class DynamicLibrary;
    };
} // namespace ecs
