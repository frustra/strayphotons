#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <iostream>
#include <queue>
#include <string>
#include <variant>

namespace ecs {
    struct Event {
        using EventData = std::variant<bool,
            char,
            int,
            float,
            double,
            glm::vec2,
            glm::vec3,
            Transform,
            EntityRef,
            Tecs::Entity,
            std::string>;

        std::string name;
        Entity source;
        EventData data;

        Event() {}
        Event(const std::string &name, const Entity &source) : name(name), source(source), data(true) {}

        template<typename T>
        Event(const std::string &name, const Entity &source, T data) : name(name), source(source), data(data) {}

        std::string toString() const;
    };

    std::ostream &operator<<(std::ostream &out, const Event::EventData &v);

    class EventQueue {
    public:
        EventQueue() {}

        void Add(const Event &event);
        bool Empty();
        size_t Size();
        bool Poll(Event &eventOut);

    private:
        std::queue<Event> events;
        sp::LockFreeMutex mutex;
    };

    using EventQueueRef = std::shared_ptr<EventQueue>;

    EventQueueRef NewEventQueue();
} // namespace ecs
