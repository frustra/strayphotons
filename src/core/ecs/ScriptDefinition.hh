/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Events.hh"
#include "game/SceneRef.hh"
#include "graphics/GenericCompositor.hh"

#include <functional>
#include <map>
#include <variant>

namespace ecs {
    class ScriptState;
    struct GuiElement;

    using LogicUpdateLock = Lock<SendEventsLock,
        Read<TransformSnapshot, VoxelArea, SceneInfo>,
        Write<TransformTree,
            ActiveScene,
            Audio,
            Renderable,
            Light,
            LightSensor,
            LaserEmitter,
            LaserLine,
            Physics,
            PhysicsQuery,
            PhysicsJoints,
            Signals>>;

    using PhysicsUpdateLock = Lock<SendEventsLock,
        ReadSignalsLock,
        Read<TransformTree, Physics, SceneInfo>,
        Write<TransformSnapshot, OpticalElement, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>>;

    using GuiUpdateLock = Lock<ReadSignalsLock, Read<EventInput, RenderOutput, Scripts>>;

    using ScriptInitFunc = std::function<void(ScriptState &)>;
    using ScriptDestroyFunc = std::function<void(ScriptState &)>;
    using LogicTickFunc = std::function<void(ScriptState &, const LogicUpdateLock &, Entity, chrono_clock::duration)>;
    using PhysicsTickFunc = std::function<
        void(ScriptState &, const PhysicsUpdateLock &, Entity, chrono_clock::duration)>;
    using OnEventFunc = std::function<void(ScriptState &, const DynamicLock<SendEventsLock> &, Entity, Event)>;
    using PrefabFunc = std::function<void(const ScriptState &, const sp::SceneRef &, Lock<AddRemove>, Entity)>;
    using BeforeFrameFunc = std::function<bool(ScriptState &, Entity)>;
    using RenderGuiFunc = std::function<sp::GuiDrawData(ScriptState &, Entity, glm::vec2, glm::vec2, float)>;
    using GuiRenderFuncs = std::pair<BeforeFrameFunc, RenderGuiFunc>;

    using ScriptCallback = std::
        variant<std::monostate, LogicTickFunc, PhysicsTickFunc, OnEventFunc, PrefabFunc, GuiRenderFuncs>;

    using ScriptName = sp::InlineString<63>;

    enum class ScriptType {
        LogicScript,
        PhysicsScript,
        EventScript,
        PrefabScript,
        GuiScript,
    };

    struct ScriptDefinitionBase {
        const StructMetadata &metadata;

        ScriptDefinitionBase(const StructMetadata &metadata) : metadata(metadata) {}
        virtual const void *GetDefault() const = 0;
        virtual void *AccessMut(ScriptState &state) const = 0;
        virtual const void *Access(const ScriptState &state) const = 0;
    };

    static StructMetadata MetadataScriptDefinitionBase(typeid(ScriptDefinitionBase),
        sizeof(ScriptDefinitionBase),
        "ScriptDefinitionBase",
        "A generic script context object base class",
        StructFunction::New("AccessMut", "Return a pointer to the script state data", &ScriptDefinitionBase::AccessMut),
        StructFunction::New("Access",
            "Return a const pointer to the script state data",
            &ScriptDefinitionBase::Access));

    struct ScriptDefinition {
        ScriptName name;
        ScriptType type;
        std::vector<EventName> events;
        bool filterOnEvent = false;
        std::weak_ptr<ScriptDefinitionBase> context;
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
            &ScriptDefinition::filterOnEvent));

    struct ScriptDefinitions {
        std::map<ScriptName, ScriptDefinition> scripts;

        void RegisterScript(ScriptDefinition &&definition);
    };

    ScriptDefinitions &GetScriptDefinitions();
} // namespace ecs
