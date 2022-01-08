#pragma once

#include "core/EnumArray.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vector>

namespace ecs {
    enum class TriggerGroup : uint8_t {
        Player = 0,
        Object,
        Count,
    };

    struct TriggerArea {
        struct TriggerCommand {
            std::string command;
            robin_hood::unordered_flat_set<Tecs::Entity> executed_entities;

            TriggerCommand(const std::string &command) : command(command) {}
        };
        struct TriggerSignal {
            std::string outputSignal;
            TriggerSignal(const std::string &signal) : outputSignal(signal) {
                sizeof(TriggerArea);
                sizeof(triggers);
                sizeof(contained_entities);
                sizeof(std::vector<std::variant<TriggerCommand, TriggerSignal>>);
            }
        };

        sp::EnumArray<std::vector<std::variant<TriggerCommand, TriggerSignal>>, TriggerGroup> triggers;
        robin_hood::unordered_flat_set<Tecs::Entity> contained_entities;
    };

    static Component<TriggerGroup> ComponentTriggerGroup("trigger_group");
    static Component<TriggerArea> ComponentTriggerArea("trigger_area");

    template<>
    bool Component<TriggerGroup>::Load(sp::Scene *scene, TriggerGroup &dst, const picojson::value &src);
    template<>
    bool Component<TriggerArea>::Load(sp::Scene *scene, TriggerArea &dst, const picojson::value &src);
} // namespace ecs
