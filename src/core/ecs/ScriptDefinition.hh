/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Tecs_permissions.hh"
#include "ecs/Ecs.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Events.hh"
#include "game/SceneRef.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/gui/GuiDrawData.hh"
#include "strayphotons/gui/GuiInputEvents.hh"

#include <map>

namespace sp {
    class GenericCompositor;
} // namespace sp

namespace ecs {
    class ScriptState;
    struct GuiElement;

    using LogicUpdateLock = Lock<SendEventsLock,
        Read<TransformSnapshot, VoxelArea, LightCast, SceneInfo, SceneProperties>,
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
            Signals>,
        ReadPrevious>;

    using PhysicsUpdateLock = Lock<SendEventsLock,
        ReadSignalsLock,
        Read<TransformTree, Physics, SceneInfo>,
        Write<TransformSnapshot, OpticalElement, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>,
        ReadPrevious>;

    using ScriptInitFunc = void (*)(ScriptState &);
    using ScriptDestroyFunc = void (*)(ScriptState &);
    using LogicTickFunc = void (*)(ScriptState &, const LogicUpdateLock &, Entity, chrono_clock::duration);
    using PhysicsTickFunc = void (*)(ScriptState &, const PhysicsUpdateLock &, Entity, chrono_clock::duration);
    using OnEventFunc = void (*)(ScriptState &, const DynamicLock<SendEventsLock> &, Entity, Event);
    using PrefabFunc = void (*)(const ScriptState &, const sp::SceneRef &, Lock<AddRemove>, Entity);
    using BeforeFrameFunc = bool (*)(sp::GenericCompositor &,
        ScriptState &,
        Entity,
        const sp::HeapVector<sp::GuiInputEvent> &);
    using RenderGuiFunc =
        void (*)(sp::GenericCompositor &, ScriptState &, Entity, glm::vec2, glm::vec2, float, sp::GuiDrawData &);

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

    template<typename Func>
    struct LockedFunction {
        Func callback = nullptr;
        PermissionBitset readPermissions, writePermissions;

        bool operator==(const LockedFunction<Func> &other) const = default;

        explicit operator bool() const {
            return callback;
        }
    };

    struct ScriptDefinition {
        sp::HeapString name;
        sp::HeapVector<EventName> events;
        std::weak_ptr<ScriptDefinitionBase> context;
        ScriptInitFunc initFunc = nullptr;
        ScriptDestroyFunc destroyFunc = nullptr;
        LockedFunction<LogicTickFunc> logicTickFunc = {};
        LockedFunction<PhysicsTickFunc> physicsTickFunc = {};
        LockedFunction<OnEventFunc> onEventFunc = {};
        PrefabFunc prefabFunc = nullptr;
        BeforeFrameFunc beforeFrameFunc = nullptr;
        RenderGuiFunc renderGuiFunc = nullptr;

        bool NeedsEventQueue() const {
            return !onEventFunc && (logicTickFunc || physicsTickFunc || beforeFrameFunc || renderGuiFunc);
        }
        explicit operator bool() const {
            return context.use_count() > 0 || initFunc || destroyFunc || logicTickFunc || physicsTickFunc ||
                   onEventFunc || prefabFunc || beforeFrameFunc || renderGuiFunc;
        }
    };

    static StructMetadata MetadataScriptDefinition(typeid(ScriptDefinition),
        sizeof(ScriptDefinition),
        "ScriptDefinition",
        "A definition describing the name, type, and functions of a script",
        StructField::New("name", "The name of the script", &ScriptDefinition::name),
        StructField::New("events", "A list of the names of events this script can receive", &ScriptDefinition::events));

    struct ScriptDefinitions {
        std::map<sp::HeapString, ScriptDefinition> scripts;

        void RegisterScript(ScriptDefinition &&definition);
    };

    ScriptDefinitions &GetScriptDefinitions();
} // namespace ecs
