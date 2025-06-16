/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

// #include "common/Common.hh"
// #include "common/LockFreeMutex.hh"
// #include "common/Logging.hh"
// #include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Events.hh"

#include <functional>
#include <map>
#include <variant>

namespace ecs {
    class ScriptState;

    using PhysicsUpdateLock = Lock<SendEventsLock,
        ReadSignalsLock,
        Read<TransformSnapshot, SceneInfo>,
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>>;

    using ScriptInitFunc = std::function<void(ScriptState &)>;
    using ScriptDestroyFunc = std::function<void(ScriptState &)>;
    using OnTickFunc = std::function<
        void(ScriptState &, const DynamicLock<ReadSignalsLock> &, Entity, chrono_clock::duration)>;
    using OnEventFunc = std::function<void(ScriptState &, const DynamicLock<SendEventsLock> &, Entity, Event)>;
    using PrefabFunc = std::function<void(const ScriptState &, const sp::SceneRef &, Lock<AddRemove>, Entity)>;
    using ScriptCallback = std::variant<std::monostate, OnTickFunc, OnEventFunc, PrefabFunc>;

    enum class ScriptType {
        LogicScript = 0,
        PhysicsScript,
        EventScript,
        PrefabScript,
    };

    struct ScriptDefinitionBase {
        const StructMetadata &metadata;

        ScriptDefinitionBase(const StructMetadata &metadata) : metadata(metadata) {}
        virtual const void *GetDefault() const = 0;
        virtual void *AccessMut(ScriptState &state) const = 0;
        virtual const void *Access(const ScriptState &state) const = 0;

        size_t GetSize() const {
            return metadata.size;
        }
    };

    static StructMetadata MetadataScriptDefinitionBase(typeid(ScriptDefinitionBase),
        sizeof(ScriptDefinitionBase),
        "ScriptDefinitionBase",
        "A generic script context object base class",
        StructFunction::New("AccessMut", "Return a pointer to the script state data", &ScriptDefinitionBase::AccessMut),
        StructFunction::New("Access", "Return a const pointer to the script state data", &ScriptDefinitionBase::Access),
        StructFunction::New("GetSize",
            "Return the size of the script state data in bytes",
            &ScriptDefinitionBase::GetSize));

    struct ScriptDefinition {
        std::string name;
        ScriptType type;
        std::vector<std::string> events;
        bool filterOnEvent = false;
        ScriptDefinitionBase *context = nullptr;
        std::optional<ScriptInitFunc> initFunc;
        std::optional<ScriptDestroyFunc> destroyFunc;
        ScriptCallback callback;
    };

    static StructMetadata MetadataScriptDefinition(typeid(ScriptDefinition),
        sizeof(ScriptDefinition),
        "ScriptDefinition",
        "A definition describing the name, type, and functions of a script",
        StructField::New("name", "The name of the script", &ScriptDefinition::name),
        StructField::New("type", "The type of the script", &ScriptDefinition::type),
        StructField::New("events", "A list of the names of events this script can receive", &ScriptDefinition::events),
        StructField::New("filter_on_event",
            "True if this script should only run if new events are received",
            &ScriptDefinition::filterOnEvent),
        StructField::New("context",
            "A pointer to the script context object defining this script",
            &ScriptDefinition::context));

    struct ScriptDefinitions {
        std::map<std::string, ScriptDefinition> scripts;

        void RegisterScript(ScriptDefinition &&definition);
    };

    ScriptDefinitions &GetScriptDefinitions();
} // namespace ecs
