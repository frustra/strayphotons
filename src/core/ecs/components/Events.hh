#pragma once

#include <core/Common.hh>
#include <ecs/Components.hh>
#include <glm/glm.hpp>
#include <queue>
#include <robin_hood.h>
#include <set>
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
        robin_hood::unordered_map<std::string, std::queue<Event>> events;

        EventInput() {}

        template<class... Args>
        EventInput(const Args &...eventList) {
            for (auto &event : {eventList...}) {
                events.emplace(event, std::queue<Event>());
            }
        }

        bool Add(const std::string binding, const Event &event);
        bool Poll(const std::string binding, Event &eventOut);
    };

    class EventBindings {
    public:
        EventBindings() {}

        using Binding = typename std::pair<Tecs::Entity, std::string>;
        using BindingList = typename std::vector<Binding>;

        void Bind(std::string source, Tecs::Entity target, std::string dest);
        void Unbind(std::string source, Tecs::Entity target, std::string dest);
        void UnbindSource(std::string source);
        void UnbindTarget(Tecs::Entity target);
        void UnbindDest(Tecs::Entity target, std::string dest);

        const BindingList *Lookup(std::string source) const;
        void SendEvent(Lock<Write<EventInput>> lock, const Event &event) const;
        template<typename T>
        inline void SendEvent(Lock<Write<EventInput>> lock,
                              const std::string &name,
                              Tecs::Entity &source,
                              T data) const {
            SendEvent(lock, Event(name, source, data));
        }

    private:
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    static Component<EventInput> ComponentEventInput("event_input");
    static Component<EventBindings> ComponentEventBindings("event_bindings");
} // namespace ecs
