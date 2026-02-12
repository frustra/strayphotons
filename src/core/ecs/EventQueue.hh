/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "common/InlineVector.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <array>
#include <atomic>
#include <glm/glm.hpp>
#include <iostream>
#include <queue>
#include <string>
#include <variant>

namespace ecs {
    using SendEventsLock = Lock<
        Read<Name, FocusLock, EventBindings, EventInput, Signals, SignalBindings, SignalOutput>>;

    using EventName = sp::InlineString<127>;
    using EventString = sp::InlineString<255>;
    using EventBytes = std::array<uint8_t, 256>;

    enum class EventDataType {
        Bool = 0,
        Int,
        Uint,
        Float,
        Double,
        Vec2,
        Vec3,
        Vec4,
        Transform,
        NamedEntity,
        Entity,
        String,
        Bytes
    };

    struct EventData {
        EventDataType type;
        union {
            bool b;
            int i;
            unsigned int ui;
            float f;
            double d;
            glm::vec2 vec2;
            glm::vec3 vec3;
            glm::vec4 vec4;
            Transform transform;
            NamedEntity namedEntity;
            Entity ent;
            EventString str;
            EventBytes bytes;
        };

        EventData() : type(EventDataType::Bool), b(false) {}
        EventData(bool b) : type(EventDataType::Bool), b(b) {}
        EventData(int i) : type(EventDataType::Int), i(i) {}
        EventData(unsigned int ui) : type(EventDataType::Uint), ui(ui) {}
        EventData(float f) : type(EventDataType::Float), f(f) {}
        EventData(double d) : type(EventDataType::Double), d(d) {}
        EventData(const glm::vec2 &vec2) : type(EventDataType::Vec2), vec2(vec2) {}
        EventData(const glm::vec3 &vec3) : type(EventDataType::Vec3), vec3(vec3) {}
        EventData(const glm::vec4 &vec4) : type(EventDataType::Vec4), vec4(vec4) {}
        EventData(const Transform &transform) : type(EventDataType::Transform), transform(transform) {}
        EventData(const NamedEntity &namedEntity) : type(EventDataType::NamedEntity), namedEntity(namedEntity) {}
        EventData(Entity ent) : type(EventDataType::Entity), ent(ent) {}
        EventData(const EventString &str) : type(EventDataType::String), str(str) {}
        EventData(const EventBytes &bytes) : type(EventDataType::Bytes), bytes(bytes) {}

        template<typename T, typename EventT>
        static auto *TryGet(EventT &event) {
            if constexpr (std::is_same<T, bool>()) {
                if (event.type == EventDataType::Bool) return &event.b;
            } else if constexpr (std::is_same<T, int>()) {
                if (event.type == EventDataType::Int) return &event.i;
            } else if constexpr (std::is_same<T, unsigned int>()) {
                if (event.type == EventDataType::Uint) return &event.ui;
            } else if constexpr (std::is_same<T, float>()) {
                if (event.type == EventDataType::Float) return &event.f;
            } else if constexpr (std::is_same<T, double>()) {
                if (event.type == EventDataType::Double) return &event.d;
            } else if constexpr (std::is_same<T, glm::vec2>()) {
                if (event.type == EventDataType::Vec2) return &event.vec2;
            } else if constexpr (std::is_same<T, glm::vec3>()) {
                if (event.type == EventDataType::Vec3) return &event.vec3;
            } else if constexpr (std::is_same<T, glm::vec4>()) {
                if (event.type == EventDataType::Vec4) return &event.vec4;
            } else if constexpr (std::is_same<T, Transform>()) {
                if (event.type == EventDataType::Transform) return &event.transform;
            } else if constexpr (std::is_same<T, NamedEntity>()) {
                if (event.type == EventDataType::NamedEntity) return &event.namedEntity;
            } else if constexpr (std::is_same<T, Entity>()) {
                if (event.type == EventDataType::Entity) return &event.ent;
            } else if constexpr (std::is_same<T, EventString>()) {
                if (event.type == EventDataType::String) return &event.str;
            } else if constexpr (std::is_same<T, EventBytes>()) {
                if (event.type == EventDataType::Bytes) return &event.bytes;
            } else {
                static_assert(false, "Unexpected EventData type");
            }
            return (std::conditional_t<std::is_const_v<EventT>, const T, T> *)nullptr;
        }

        template<typename T, typename EventT>
        static auto &Get(EventT &event) {
            auto *ptr = TryGet<T, EventT>(event);
            Assertf(ptr != nullptr, "Unexpected EventData type: %s != %s", typeid(T).name(), event.type);
            return *ptr;
        }

        template<typename Fn, typename EventT>
        static auto Visit(EventT &event, Fn &&callable) {
            switch (event.type) {
            case EventDataType::Bool:
                return callable(event.b);
            case EventDataType::Int:
                return callable(event.i);
            case EventDataType::Uint:
                return callable(event.ui);
            case EventDataType::Float:
                return callable(event.f);
            case EventDataType::Double:
                return callable(event.d);
            case EventDataType::Vec2:
                return callable(event.vec2);
            case EventDataType::Vec3:
                return callable(event.vec3);
            case EventDataType::Vec4:
                return callable(event.vec4);
            case EventDataType::Transform:
                return callable(event.transform);
            case EventDataType::NamedEntity:
                return callable(event.namedEntity);
            case EventDataType::Entity:
                return callable(event.ent);
            case EventDataType::String:
                return callable(event.str);
            case EventDataType::Bytes:
                return callable(event.bytes);
            default:
                Abortf("Unexpected EventData type: %s", event.type);
            }
        }

        const EventData &operator=(bool newBool);
        const EventData &operator=(int newInt);
        const EventData &operator=(unsigned int newUint);
        const EventData &operator=(float newFloat);
        const EventData &operator=(double newDouble);
        const EventData &operator=(glm::vec2 newVec2);
        const EventData &operator=(glm::vec3 newVec3);
        const EventData &operator=(glm::vec4 newVec4);
        const EventData &operator=(Transform newTransform);
        const EventData &operator=(NamedEntity newNamedEntity);
        const EventData &operator=(Entity newEntity);
        const EventData &operator=(EventString newString);
        const EventData &operator=(EventBytes newBytes);

        bool operator==(const EventData &other) const;
    };

    using EventDataVariant = std::variant<bool,
        int,
        unsigned int,
        float,
        double,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        Transform,
        NamedEntity,
        Tecs::Entity,
        EventString,
        EventBytes>;

    static StructMetadata MetadataEventData(typeid(EventData),
        sizeof(EventData),
        "EventData",
        "Stores a variety of possible data types for sending in events "
        "(JSON supported values are: **bool**, **double**, **vec2**, **vec3**, **vec4**, and **string**).",
        StructField::New("type", &EventData::type, FieldAction::None),
        StructField::New("b", &EventData::b, FieldAction::None),
        StructField::New("i", &EventData::i, FieldAction::None),
        StructField::New("ui", &EventData::ui, FieldAction::None),
        StructField::New("f", &EventData::f, FieldAction::None),
        StructField::New("d", &EventData::d, FieldAction::None),
        StructField::New("vec2", &EventData::vec2, FieldAction::None),
        StructField::New("vec3", &EventData::vec3, FieldAction::None),
        StructField::New("vec4", &EventData::vec4, FieldAction::None),
        StructField::New("transform", &EventData::transform, FieldAction::None),
        StructField::New("namedEntity", &EventData::namedEntity, FieldAction::None),
        StructField::New("ent", &EventData::ent, FieldAction::None),
        StructField::New("str", &EventData::str, FieldAction::None),
        StructField::New("bytes", &EventData::bytes, FieldAction::None));
    template<>
    bool StructMetadata::Load<EventData>(EventData &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventData>(const EntityScope &scope,
        picojson::value &dst,
        const EventData &src,
        const EventData *def);
    template<>
    void StructMetadata::SetScope<EventData>(EventData &dst, const EntityScope &scope);

    struct Event {
        EventName name;
        Entity source;
        EventData data;

        Event() {}
        template<typename T>
        Event(std::string_view name, const Entity &source, const T &data) : name(name), source(source), data(data) {}

        static uint64_t Send(const DynamicLock<SendEventsLock> &lock, Entity target, const Event &event);
        static uint64_t SendNamed(const DynamicLock<SendEventsLock> &lock,
            const NamedEntity &target,
            const Event &event);
        static uint64_t SendRef(const DynamicLock<SendEventsLock> &lock, const EntityRef &target, const Event &event);

        std::string ToString() const;
    };

    static StructMetadata MetadataEvent(typeid(Event),
        sizeof(Event),
        "Event",
        "A named event with data originating from a source entity.",
        StructField::New("name", "The event name", &Event::name),
        StructField::New("source", "The entity the event originated from", &Event::source),
        StructField::New("data", "The event payload", &Event::data),
        StructFunction::New("Send", "", &Event::Send, ArgDesc("lock", ""), ArgDesc("target", ""), ArgDesc("event", "")),
        StructFunction::New("SendRef",
            "",
            &Event::SendRef,
            ArgDesc("lock", ""),
            ArgDesc("target", ""),
            ArgDesc("event", "")));

    struct AsyncEvent {
        EventName name;
        Entity source;
        sp::AsyncPtr<EventData> data;

        uint64_t transactionId = 0;

        AsyncEvent() {}
        AsyncEvent(std::string_view name, const Entity &source, const sp::AsyncPtr<EventData> &data)
            : name(name), source(source), data(data) {}

        template<typename T>
        AsyncEvent(std::string_view name, const Entity &source, T data)
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
        static const size_t QUEUE_POOL_BLOCK_SIZE = 1024;

        using Ref = std::shared_ptr<EventQueue>;
        using WeakRef = std::weak_ptr<EventQueue>;

        // Returns false if the queue is full
        bool Add(const AsyncEvent &event);
        // Returns false if the queue is full
        bool Add(const Event &event, uint64_t transactionId = 0);

        // Returns false if the queue is empty
        bool Poll(Event &eventOut, uint64_t transactionId = 0);

        bool Empty();
        void Clear();
        uint32_t Size();

        uint32_t Capacity() const {
            return events.size();
        }

        static Ref New(uint32_t maxQueueSize = EventQueue::MAX_QUEUE_SIZE);

        EventQueue() : events(0), state({0, 0}) {}

    private:
        struct State {
            // Workaround for Clang so that std::atomic<State> operations can be inlined as if uint64. See issue:
            // https://stackoverflow.com/questions/60445848/clang-doesnt-inline-stdatomicload-for-loading-64-bit-structs
            alignas(std::atomic_uint64_t) uint32_t head;
            uint32_t tail;
        };

        sp::InlineVector<AsyncEvent, MAX_QUEUE_SIZE> events;
        std::atomic<State> state;
        size_t poolIndex;
    };

    using EventQueueRef = EventQueue::Ref;
    using EventQueueWeakRef = EventQueue::WeakRef;
} // namespace ecs

namespace std {
    // Thread-safe equality check without weak_ptr::lock()
    inline bool operator==(const ecs::EventQueueRef &a, const ecs::EventQueueWeakRef &b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }
} // namespace std
