#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>

namespace ecs {
    enum class TriggerGroup : uint8_t {
        PLAYER = 0,
        OBJECT,
        COUNT,
    };

    struct TriggerArea {
        glm::vec3 boundsMin, boundsMax;

        struct Trigger {
            std::string command, signalOutput;

            robin_hood::unordered_flat_set<Tecs::Entity> entities;
        };
        std::array<Trigger, (size_t)TriggerGroup::COUNT> triggers;
    };

    static Component<TriggerGroup> ComponentTriggerGroup("trigger_group");
    static Component<TriggerArea> ComponentTriggerArea("trigger_area");

    template<>
    bool Component<TriggerGroup>::Load(sp::Scene *scene, TriggerGroup &dst, const picojson::value &src);
    template<>
    bool Component<TriggerArea>::Load(sp::Scene *scene, TriggerArea &dst, const picojson::value &src);
} // namespace ecs
