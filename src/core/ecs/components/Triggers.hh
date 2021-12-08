#pragma once

#include "core/EnumArray.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>

namespace ecs {
    enum class TriggerGroup : uint8_t {
        Player = 0,
        Object,
        Count,
    };

    struct TriggerArea {
        struct Trigger {
            std::string command, signalOutput;

            robin_hood::unordered_flat_set<Tecs::Entity> entities;
        };
        sp::EnumArray<Trigger, TriggerGroup> triggers;
    };

    static Component<TriggerGroup> ComponentTriggerGroup("trigger_group");
    static Component<TriggerArea> ComponentTriggerArea("trigger_area");

    template<>
    bool Component<TriggerGroup>::Load(sp::Scene *scene, TriggerGroup &dst, const picojson::value &src);
    template<>
    bool Component<TriggerArea>::Load(sp::Scene *scene, TriggerArea &dst, const picojson::value &src);
} // namespace ecs
