#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>
#include <queue>
#include <robin_hood.h>
#include <string>
#include <variant>

namespace ecs {
    struct Event {
        using EventData = std::variant<bool, char, int, double, glm::vec2, Tecs::Entity, std::string>;
        std::string name;
        Tecs::Entity source;
        EventData data;

        Event() {}
        Event(const std::string &name, Tecs::Entity &source) : name(name), source(source), data(true) {}

        template<typename T>
        Event(const std::string &name, Tecs::Entity &source, T data) : name(name), source(source), data(data) {}
    };

    struct EventInput {
        robin_hood::unordered_flat_map<std::string, std::queue<Event>> events;

        EventInput() {}

        template<class... Args>
        EventInput(const Args &...eventList) {
            for (auto &event : {eventList...}) {
                events.emplace(event, std::queue<Event>());
            }
        }

        bool Add(const std::string binding, const Event &event) {
            auto queue = events.find(binding);
            if (queue != events.end()) {
                queue->second.emplace(event);
                return true;
            }
            return false;
        }

        bool Poll(const std::string binding, Event &eventOut) {
            auto queue = events.find(binding);
            if (queue != events.end() && !queue->second.empty()) {
                eventOut = queue->second.front();
                queue->second.pop();
                return true;
            }
            eventOut = Event();
            return false;
        }
    };

    static Component<EventInput> ComponentEventInput("event_input");
} // namespace ecs
