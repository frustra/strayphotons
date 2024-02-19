/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/LockFreeMutex.hh"
#include "common/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/components/Events.hh"
#include "ecs/components/Signals.hh"
#include "game/SceneRef.hh"

#include <vector>

namespace ecs {
    class ScriptInstance {
    public:
        ScriptInstance() {}
        ScriptInstance(const std::shared_ptr<ScriptState> &state) : state(state) {}
        ScriptInstance(const EntityScope &scope, const ScriptDefinition &definition)
            : ScriptInstance(GetScriptManager()->NewScriptInstance(scope, definition)) {}
        ScriptInstance(const EntityScope &scope, OnTickFunc callback)
            : ScriptInstance(scope, ScriptDefinition{"", {}, false, nullptr, {}, callback}) {}
        ScriptInstance(const EntityScope &scope, OnPhysicsUpdateFunc callback)
            : ScriptInstance(scope, ScriptDefinition{"", {}, false, nullptr, {}, callback}) {}
        ScriptInstance(const EntityScope &scope, PrefabFunc callback)
            : ScriptInstance(scope, ScriptDefinition{"", {}, false, nullptr, {}, callback}) {}

        explicit operator bool() const {
            return state && *state;
        }

        // Compare script definition and parameters
        bool operator==(const ScriptInstance &other) const {
            return state && other.state && *state == *other.state;
        }

        // Returns true if the two scripts should represent the same instance
        bool CompareOverride(const ScriptInstance &other) const {
            return state && other.state && state->CompareOverride(*other.state);
        }

        size_t GetInstanceId() const {
            if (!state) return 0;
            return state->instanceId;
        }

        void SetScope(const EntityScope &scope);

        std::shared_ptr<ScriptState> state;
    };
    static StructMetadata MetadataScriptInstance(typeid(ScriptInstance),
        "ScriptInstance",
        R"(
Script instances contain a script definition (referenced by name), and a list of parameters as input for the script state.  
Scripts can have 2 types: 
- "prefab": Prefab scripts such as "template" will run during scene load.
- "onTick": OnTick scripts (or Runtime scripts) will run during in GameLogic thread during its frame.  
            OnTick scripts starting with "physics_" will run in the Physics thread just before simulation.  
            Some OnTick scripts may also internally define event filters to only run when events are received.

Script instances are defined using the following fields:
| Instance Field | Type    | Description |
|----------------|---------|-------------|
| **prefab**     | string  | The name of a [Prefab Script](Prefab_Scripts.md) |
| **onTick**     | string  | The name of a [Runtime Script](Runtime_Scripts.md) |
| **parameters** | *any*   | A set of parameters to be given to the script. See individiaul script documentation for info. |

Here is an example of an instance definition for a "spotlight" [`template` Prefab](Prefab_Scripts.md#template-prefab):
```json
{
    "prefab": "template",
    "parameters": {
        "source": "spotlight"
    }
}
```
)");
    template<>
    bool StructMetadata::Load<ScriptInstance>(ScriptInstance &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<ScriptInstance>(const EntityScope &scope,
        picojson::value &dst,
        const ScriptInstance &src,
        const ScriptInstance *def);
    template<>
    void StructMetadata::SetScope<ScriptInstance>(ScriptInstance &dst, const EntityScope &scope);

    struct Scripts {
        ScriptState &AddOnTick(const EntityScope &scope, const std::string &scriptName) {
            return *(scripts.emplace_back(scope, GetScriptDefinitions()->scripts.at(scriptName)).state);
        }
        ScriptState &AddPrefab(const EntityScope &scope, const std::string &scriptName) {
            return *(scripts.emplace_back(scope, GetScriptDefinitions()->prefabs.at(scriptName)).state);
        }

        ScriptState &AddOnTick(const EntityScope &scope, OnTickFunc callback) {
            return *(scripts.emplace_back(scope, callback).state);
        }
        ScriptState &AddOnPhysicsUpdate(const EntityScope &scope, OnPhysicsUpdateFunc callback) {
            return *(scripts.emplace_back(scope, callback).state);
        }
        ScriptState &AddPrefab(const EntityScope &scope, PrefabFunc callback) {
            return *(scripts.emplace_back(scope, callback).state);
        }

        const ScriptState *FindScript(size_t instanceId) const;

        std::vector<ScriptInstance> scripts;
    };

    static StructMetadata MetadataScripts(typeid(Scripts),
        "Scripts",
        "",
        StructField::New(&Scripts::scripts, ~FieldAction::AutoApply));
    static Component<Scripts> ComponentScripts(MetadataScripts, "script");

    template<>
    void Component<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget);

    // Cheecks if the script has an Init(ScriptState &state) function
    template<typename T, typename = void>
    struct script_has_init_func : std::false_type {};
    template<typename T>
    struct script_has_init_func<T, std::void_t<decltype(std::declval<T>().Init(std::declval<ScriptState &>()))>>
        : std::true_type {};

    template<typename T>
    struct InternalScript final : public InternalScriptBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *Access(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.userData);
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnTick(state, lock, ent, interval);
        }

        InternalScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions()->RegisterScript({name, {}, false, this, ScriptInitFunc(&Init), OnTickFunc(&OnTick)});
        }

        template<typename... Events>
        InternalScript(const std::string &name, const StructMetadata &metadata, bool filterOnEvent, Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions()->RegisterScript(
                {name, {events...}, filterOnEvent, this, ScriptInitFunc(&Init), OnTickFunc(&OnTick)});
        }
    };

    template<typename T>
    struct InternalPhysicsScript final : public InternalScriptBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *Access(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.userData);
            return ptr ? ptr : &defaultValue;
        }

        static void Init(ScriptState &state) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            if constexpr (script_has_init_func<T>()) ptr->Init(state);
        }

        static void OnPhysicsUpdate(ScriptState &state,
            PhysicsUpdateLock lock,
            Entity ent,
            chrono_clock::duration interval) {
            T *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            ptr->OnPhysicsUpdate(state, lock, ent, interval);
        }

        InternalPhysicsScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions()->RegisterScript(
                {name, {}, false, this, ScriptInitFunc(&Init), OnPhysicsUpdateFunc(&OnPhysicsUpdate)});
        }

        template<typename... Events>
        InternalPhysicsScript(const std::string &name,
            const StructMetadata &metadata,
            bool filterOnEvent,
            Events... events)
            : InternalScriptBase(metadata) {
            GetScriptDefinitions()->RegisterScript(
                {name, {events...}, filterOnEvent, this, ScriptInitFunc(&Init), OnPhysicsUpdateFunc(&OnPhysicsUpdate)});
        }
    };

    template<typename T>
    struct PrefabScript final : public InternalScriptBase {
        const T defaultValue = {};

        const void *GetDefault() const override {
            return &defaultValue;
        }

        void *Access(ScriptState &state) const override {
            void *ptr = std::any_cast<T>(&state.userData);
            if (!ptr) ptr = &state.userData.emplace<T>();
            return ptr;
        }

        const void *Access(const ScriptState &state) const override {
            const void *ptr = std::any_cast<T>(&state.userData);
            return ptr ? ptr : &defaultValue;
        }

        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
            const T *ptr = std::any_cast<T>(&state.userData);
            T data;
            if (ptr) data = *ptr;
            data.Prefab(state, scene.Lock(), lock, ent);
        }

        PrefabScript(const std::string &name, const StructMetadata &metadata) : InternalScriptBase(metadata) {
            GetScriptDefinitions()->RegisterPrefab({name, {}, false, this, {}, PrefabFunc(&Prefab)});
        }
    };
} // namespace ecs
