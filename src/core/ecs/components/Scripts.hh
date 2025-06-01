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
            : ScriptInstance(GetScriptManager().NewScriptInstance(scope, definition)) {}

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
        sizeof(ScriptInstance),
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
        ScriptState &AddScript(const EntityScope &scope, const ScriptDefinition &definition) {
            return *(scripts.emplace_back(scope, definition).state);
        }
        ScriptState &AddScript(const EntityScope &scope, const std::string &scriptName) {
            return *(scripts.emplace_back(scope, GetScriptDefinitions().scripts.at(scriptName)).state);
        }
        ScriptState &AddPrefab(const EntityScope &scope, const std::string &scriptName) {
            return *(scripts.emplace_back(scope, GetScriptDefinitions().prefabs.at(scriptName)).state);
        }

        const ScriptState *FindScript(size_t instanceId) const;

        std::vector<ScriptInstance> scripts;
    };

    static EntityComponent<Scripts> ComponentScripts(
        {typeid(Scripts), sizeof(Scripts), "Scripts", "", StructField::New(&Scripts::scripts, ~FieldAction::AutoApply)},
        "script");

    template<>
    void EntityComponent<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget);
} // namespace ecs
