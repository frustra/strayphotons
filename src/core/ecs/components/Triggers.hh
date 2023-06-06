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

    enum class TriggerShape : uint8_t { Box = 0, Sphere = 1 };

    static sp::EnumArray<std::string, TriggerGroup> TriggerGroupSignalNames = {
        "trigger_player_count",
        "trigger_object_count",
        "trigger_magnetic_count",
    };

    static sp::EnumArray<std::pair<std::string, std::string>, TriggerGroup> TriggerGroupEventNames = {
        std::make_pair("/trigger/player/enter", "/trigger/player/leave"),
        std::make_pair("/trigger/object/enter", "/trigger/object/leave"),
        std::make_pair("/trigger/magnetic/enter", "/trigger/magnetic/leave"),
    };

    struct TriggerArea {
        TriggerShape shape = TriggerShape::Box;
        sp::EnumArray<robin_hood::unordered_flat_set<Entity>, TriggerGroup> containedEntities;
    };

    static StructMetadata MetadataTriggerGroup(typeid(TriggerGroup),
        "trigger_group",
        "",
        StructField::New<TriggerGroup>());
    static Component<TriggerGroup> ComponentTriggerGroup(MetadataTriggerGroup);

    static StructMetadata MetadataTriggerArea(typeid(TriggerArea),
        "trigger_area",
        "",
        StructField::New(&TriggerArea::shape));
    static Component<TriggerArea> ComponentTriggerArea(MetadataTriggerArea);
} // namespace ecs
