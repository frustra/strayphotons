/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Async.hh"
#include "core/InlineVector.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <atomic>
#include <glm/glm.hpp>
#include <iostream>
#include <queue>
#include <string>
#include <variant>

namespace ecs {
    using EventData = std::variant<bool,
        char,
        int,
        float,
        double,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        Transform,
        EntityRef,
        Tecs::Entity,
        std::string>;

    static StructMetadata MetadataEventData(typeid(EventData),
        "EventData",
        "Stores a variety of possible data types for sending in events "
        "(JSON supported values are: **bool**, **double**, **vec2**, **vec3**, **vec4**, and **string**).");
    template<>
    bool StructMetadata::Load<EventData>(EventData &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventData>(const EntityScope &scope,
        picojson::value &dst,
        const EventData &src,
        const EventData *def);

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
        static const size_t MAX_QUEUE_SIZE = 1000;

        EventQueue(uint32_t maxQueueSize) : events(maxQueueSize), state({0, 0}) {
            Assertf(maxQueueSize <= MAX_QUEUE_SIZE,
                "EventQueue size %u exceeds max size %u",
                maxQueueSize,
                MAX_QUEUE_SIZE);
        }

        // Returns false if the queue is full
        bool Add(const AsyncEvent &event);
        // Returns false if the queue is full
        bool Add(const Event &event, size_t transactionId = 0);

        // Returns false if the queue is empty
        bool Poll(Event &eventOut, size_t transactionId = 0);

        bool Empty();
        size_t Size();

        // Not thread safe, drops all events in queue
        void Resize(uint32_t newSize);
        size_t Capacity() const {
            return events.size();
        }

    private:
        struct State {
            // Workaround for Clang so that std::atomic<State> operations can be inlined as if uint64. See issue:
            // https://stackoverflow.com/questions/60445848/clang-doesnt-inline-stdatomicload-for-loading-64-bit-structs
            alignas(std::atomic_uint64_t) uint32_t head;
            uint32_t tail;
        };

        sp::InlineVector<AsyncEvent, MAX_QUEUE_SIZE> events;
        std::atomic<State> state;
    };

    using EventQueueRef = std::shared_ptr<EventQueue>;

    EventQueueRef NewEventQueue(uint32_t maxQueueSize = EventQueue::MAX_QUEUE_SIZE);
} // namespace ecs
