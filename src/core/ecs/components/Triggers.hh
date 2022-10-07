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
        sp::EnumArray<robin_hood::unordered_flat_set<Entity>, TriggerGroup> containedEntities;
    };

    static Component<TriggerGroup> ComponentTriggerGroup("trigger_group", ComponentField::New<TriggerGroup>());
    static Component<TriggerArea> ComponentTriggerArea("trigger_area");
} // namespace ecs
