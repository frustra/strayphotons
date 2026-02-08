/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Ecs.hh"
#include "common/Common.hh"
#include "common/DispatchQueue.hh"
#include "common/Logging.hh"
#include "ecs/ComponentsImpl.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/EventQueue.hh"

// Components
#include "ecs/components/ActiveScene.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/CharacterController.hh"
#include "ecs/components/Events.hh"
#include "ecs/components/Focus.hh"
#include "ecs/components/GuiElement.hh"
#include "ecs/components/LaserEmitter.hh"
#include "ecs/components/LaserLine.hh"
#include "ecs/components/LaserSensor.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/Network.hh"
#include "ecs/components/OpticalElement.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/PhysicsJoints.hh"
#include "ecs/components/PhysicsQuery.hh"
#include "ecs/components/RenderOutput.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/SceneConnection.hh"
#include "ecs/components/SceneInfo.hh"
#include "ecs/components/SceneProperties.hh"
#include "ecs/components/Screen.hh"
#include "ecs/components/Scripts.hh"
#include "ecs/components/Signals.hh"
#include "ecs/components/Sound.hh"
#include "ecs/components/Transform.h"
#include "ecs/components/Triggers.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelArea.hh"
#include "ecs/components/XrView.hh"

namespace ecs {
    struct EventQueuePool : public sp::NonMoveable {
        sp::LogOnExit logOnExit = "EventQueuePool shut down ==============================================";
        std::mutex mutex;
        std::deque<EventQueue> pool;
        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> freeList;
    };

    struct ECSContext : public sp::NonMoveable {
        // Order of these is important! Items are destroyed in bottom-up order.
        sp::LogOnExit logOnExit = "ECS shut down =========================================================";
        EventQueuePool eventQueues;
        ECS staging;
        ECS live;
        EntityReferenceManager refManager;
        sp::DispatchQueue transactionQueue = sp::DispatchQueue("ECSTransactionQueue", 2, std::chrono::milliseconds(1));
    };

    // Define these special components here to solve circular includes
    static EntityComponent<Name> ComponentName(
        StructMetadata{
            typeid(Name),
            sizeof(Name),
            "Name",
            DocsDescriptionName,
            StructField::New("scene", &Name::scene, FieldAction::None),
            StructField::New("entity", &Name::entity, FieldAction::None),
        },
        "name");
    static EntityComponent<SceneInfo> ComponentSceneInfo("SceneInfo",
        "This is an internal component storing each entity's source scene and other creation info.");

    static StructMetadata MetadataEntity(typeid(Entity), sizeof(Entity), "Entity", "");
    static StructMetadata MetadataNamedEntity(typeid(NamedEntity),
        sizeof(NamedEntity),
        "NamedEntity",
        "",
        StructFunction::New("Name", "Returns the name of the entity being referenced", &NamedEntity::Name),
        StructFunction::New("Get",
            "Returns the actual entity being referenced",
            &NamedEntity::Get,
            ArgDesc("lock", "")),
        StructFunction::New("IsValid", "Returns true if this reference is non-empty", &NamedEntity::IsValid),
        StructFunction::New("Find", "Finds the name of an existing entity", &NamedEntity::Find, ArgDesc("ent", "")),
        StructFunction::New("Lookup",
            "Looks up an entity by name",
            &NamedEntity::Lookup,
            ArgDesc("name", ""),
            ArgDesc("scope", "")),
        StructFunction::New("Clear", "Clears the entity and sets it back to empty", &NamedEntity::Clear));

    static StructMetadata MetadataEntityRef(typeid(EntityRef),
        sizeof(EntityRef),
        "EntityRef",
        DocsDescriptionEntityRef,
        StructFunction::New("Name", "Returns the name of the entity being referenced", &EntityRef::Name),
        StructFunction::New("Get", "Returns the actual entity being referenced", &EntityRef::Get, ArgDesc("lock", "")),
        StructFunction::New("IsValid", "Returns true if this reference is non-empty", &EntityRef::IsValid),
        StructFunction::New("Empty", "Create a new empty entity reference", &EntityRef::Empty),
        StructFunction::New("New",
            "Create a new entity reference from an existing entity",
            &EntityRef::New,
            ArgDesc("ent", "")),
        StructFunction::New("Copy",
            "Create a new entity reference from an existing reference",
            &EntityRef::Copy,
            ArgDesc("ref", "")),
        StructFunction::New("Lookup",
            "Create a new entity reference by name",
            &EntityRef::Lookup,
            ArgDesc("name", ""),
            ArgDesc("scope", "")),
        StructFunction::New("Clear", "Clears the reference and sets it back to empty", &EntityRef::Clear));

    static StructMetadata MetadataSignalRef(typeid(SignalRef),
        sizeof(SignalRef),
        "SignalRef",
        "",
        StructFunction::New("GetEntity", "Returns the entity being being referenced", &SignalRef::GetEntity),
        StructFunction::New("GetSignalName", "Returns the signal name being referenced", &SignalRef::GetSignalName),
        StructFunction::New("String", "Returns the full signal path being referenced", &SignalRef::String),
        StructFunction::New("IsValid", "Returns true if this reference is non-empty", &SignalRef::IsValid),
        StructFunction::New("SetValue", "", &SignalRef::SetValue, ArgDesc("lock", ""), ArgDesc("value", "")),
        StructFunction::New("HasValue", "", &SignalRef::HasValue, ArgDesc("lock", "")),
        StructFunction::New("ClearValue", "", &SignalRef::ClearValue, ArgDesc("lock", "")),
        StructFunction::New("GetValue", "", &SignalRef::GetValue, ArgDesc("lock", "")),
        StructFunction::
            New<SignalRef, SignalExpression &, const Lock<Write<Signals>, ReadSignalsLock> &, const SignalExpression &>(
                "SetBinding",
                "",
                &SignalRef::SetBinding,
                ArgDesc("lock", ""),
                ArgDesc("expr", "")),
        StructFunction::New("HasBinding", "", &SignalRef::HasBinding, ArgDesc("lock", "")),
        StructFunction::New("ClearBinding", "", &SignalRef::ClearBinding, ArgDesc("lock", "")),
        StructFunction::New("GetBinding", "", &SignalRef::GetBinding, ArgDesc("lock", "")),
        StructFunction::New("GetSignal",
            "Evaluates the signal referenced into a discrete value",
            &SignalRef::GetSignal,
            ArgDesc("lock", ""),
            ArgDesc("depth", "")),
        StructFunction::New("Empty", "Create a new empty signal reference", &SignalRef::Empty),
        StructFunction::New("New",
            "Create a new signal reference from an entity-signal pair",
            &SignalRef::New,
            ArgDesc("ent", ""),
            ArgDesc("signal_name", "")),
        StructFunction::New("Copy",
            "Create a new signal reference from an existing reference",
            &SignalRef::Copy,
            ArgDesc("ref", "")),
        StructFunction::New("Lookup",
            "Create a new signal reference from a path string",
            &SignalRef::Lookup,
            ArgDesc("str", ""),
            ArgDesc("scope", "")),
        StructFunction::New("Clear", "Clears the reference and sets it back to empty", &SignalRef::Clear));
} // namespace ecs
