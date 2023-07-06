/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "core/EnumTypes.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vector>

namespace ecs {
    enum class TriggerGroup : uint8_t {
        Player = 0,
        Object,
        Magnetic,
    };
    static const StructMetadata::EnumDescriptions DocsEnumTriggerGroup = {
        {(uint32_t)TriggerGroup::Player, "A group for player entities."},
        {(uint32_t)TriggerGroup::Object, "A group for generic movable object entities."},
        {(uint32_t)TriggerGroup::Magnetic, "A group for magnetic entities."},
    };
    static StructMetadata MetadataTriggerGroup(typeid(TriggerGroup),
        "trigger_group",
        "An entity's `trigger_group` determines which signals and events are generated when it enters the "
        "[`trigger_area`](#trigger_area-component) of another entity (or itself if the entity is also a "
        "`trigger_area`).",
        StructField::New<TriggerGroup>(),
        &DocsEnumTriggerGroup);
    static Component<TriggerGroup> ComponentTriggerGroup(MetadataTriggerGroup);

    enum class TriggerShape : uint8_t {
        Box = 0,
        Sphere = 1,
    };
    static const StructMetadata::EnumDescriptions DocsEnumTriggerShape = {
        {(uint32_t)TriggerShape::Box,
            "A 1x1x1 meter cube (vertices at -0.5 and 0.5) centered around the entity's origin.  \n"
            "Can be visualized by adding a `box` renderable to the same entity, or using the `laser_cube` template."},
        {(uint32_t)TriggerShape::Sphere,
            "A 1.0 meter diameter sphere (0.5m radius) centered around the entity's origin.  \n"
            "Can be visualized by adding a `sphere` renderable to the same entity."},
    };
    static const StructMetadata MetadataTriggerShape(typeid(TriggerShape),
        "TriggerShape",
        "A [`trigger_area`](#trigger_area-component)'s active area is defined by its `TriggerShape`, which is scaled "
        "and positioned based on the entity's [`transform` Component](General_Components.md#transform-component)",
        &DocsEnumTriggerShape);

    static const sp::EnumArray<std::string, TriggerGroup> TriggerGroupSignalNames = {
        "trigger_player_count",
        "trigger_object_count",
        "trigger_magnetic_count",
    };

    static const sp::EnumArray<std::pair<std::string, std::string>, TriggerGroup> TriggerGroupEventNames = {
        std::make_pair("/trigger/player/enter", "/trigger/player/leave"),
        std::make_pair("/trigger/object/enter", "/trigger/object/leave"),
        std::make_pair("/trigger/magnetic/enter", "/trigger/magnetic/leave"),
    };

    struct TriggerArea {
        TriggerShape shape = TriggerShape::Box;
        sp::EnumArray<robin_hood::unordered_flat_set<Entity>, TriggerGroup> containedEntities;
    };
    static StructMetadata MetadataTriggerArea(typeid(TriggerArea),
        "trigger_area",
        R"(
When any entity with a [`trigger_group` Component](#trigger_group-component) enters or exits this area, 
an event will be generated based on the specific group.  
A count signal is also updated for each group type if this entity also has a [`signal_output` Component](General_Components.md#signal_output-component).

The generated events are in the following form:
```
/trigger/<trigger_group>/enter
/trigger/<trigger_group>/leave

Example:
/trigger/player/enter
/trigger/player/leave
```

Similarly, the signals will be set in the [`signal_output` Component](General_Components.md#signal_output-component) like this:
```
trigger_<trigger_group>_count

Example:
trigger_player_count
```

> [!NOTE] Both generated events and signals are case-sensitive (all lowercase).
)",
        StructField::New(&TriggerArea::shape));
    static Component<TriggerArea> ComponentTriggerArea(MetadataTriggerArea);
} // namespace ecs
