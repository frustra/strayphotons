#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    struct Triggerable {
        // TODO: information that help determine which trigger areas
        // will trigger for this entity
    };

    struct TriggerArea {
        glm::vec3 boundsMin, boundsMax;
        std::string command;
        bool triggered = false;
    };

    static Component<Triggerable> ComponentTriggerable("triggerable");
    static Component<TriggerArea> ComponentTriggerArea("trigger_area");

    template<>
    bool Component<Triggerable>::Load(sp::Scene *scene, Triggerable &dst, const picojson::value &src);
    template<>
    bool Component<TriggerArea>::Load(sp::Scene *scene, TriggerArea &dst, const picojson::value &src);
} // namespace ecs
