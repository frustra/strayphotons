#pragma once

#include "assets/Async.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <iostream>
#include <queue>
#include <string>
#include <variant>

namespace ecs {
    using EventData = std::
        variant<bool, char, int, float, double, glm::vec2, glm::vec3, Transform, EntityRef, Tecs::Entity, std::string>;

    struct Event {
        std::string name;
        Entity source;
        EventData data;

        Event() {}
        template<typename T>
        Event(const std::string &name, const Entity &source, const T &data) : name(name), source(source), data(data) {}

        std::string toString() const;
    };

    struct AsyncEvent {
        std::string name;
        Entity source;
        sp::AsyncPtr<EventData> data;

        size_t transactionId = 0;

        AsyncEvent() {}
        AsyncEvent(const std::string &name, const Entity &source, const sp::AsyncPtr<EventData> &data)
            : name(name), source(source), data(data) {}

        template<typename T>
        AsyncEvent(const std::string &name, const Entity &source, T data)
            : AsyncEvent(name, source, std::make_shared<sp::Async<EventData>>(std::make_shared<T>(data))) {}
    };

    std::ostream &operator<<(std::ostream &out, const EventData &v);

    /**
     * A lock-free event queue that is thread-safe for a single reader and multiple writers.
     *
     * Event availability is synchronized with transactions by providing the current transaction's id.
     */
    class EventQueue {
    public:
        EventQueue(uint32_t maxQueueSize) : events(maxQueueSize), state({0, 0}) {}

        // Returns false if the queue is full
        bool Add(const AsyncEvent &event);
        // Returns false if the queue is full
        bool Add(const Event &event, size_t transactionId = 0);

        // Returns false if the queue is empty
        bool Poll(Event &eventOut, size_t transactionId = 0);

        bool Empty();
        size_t Size();

        size_t Capacity() {
            return events.size();
        }

    private:
        struct State {
            uint32_t head, tail;
        };

        std::vector<AsyncEvent> events;
        std::atomic<State> state;
    };

    using EventQueueRef = std::shared_ptr<EventQueue>;

    EventQueueRef NewEventQueue(uint32_t maxQueueSize = 1000);
} // namespace ecs
