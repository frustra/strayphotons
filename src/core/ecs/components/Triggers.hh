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
            robin_hood::unordered_flat_set<Entity> contained_entities;

            TriggerCommand(const std::string &command) : command(command) {}

            bool operator==(const TriggerCommand &other) const {
                return command == other.command;
            }
        };
        struct TriggerSignal {
            std::string outputSignal;
            robin_hood::unordered_flat_set<Entity> contained_entities;

            TriggerSignal(const std::string &signal) : outputSignal(signal) {}

            bool operator==(const TriggerSignal &other) const {
                return outputSignal == other.outputSignal;
            }
        };

        sp::EnumArray<std::vector<std::variant<TriggerCommand, TriggerSignal>>, TriggerGroup> triggers;
    };

    static Component<TriggerGroup> ComponentTriggerGroup("trigger_group");
    static Component<TriggerArea> ComponentTriggerArea("trigger_area");

    template<>
    bool Component<TriggerGroup>::Load(ScenePtr scenePtr,
        const Name &scope,
        TriggerGroup &dst,
        const picojson::value &src);
    template<>
    bool Component<TriggerArea>::Load(ScenePtr scenePtr,
        const Name &scope,
        TriggerArea &dst,
        const picojson::value &src);
    template<>
    void Component<TriggerArea>::Apply(const TriggerArea &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
