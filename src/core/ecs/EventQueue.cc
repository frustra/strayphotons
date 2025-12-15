/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EventQueue.hh"

#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <optional>
#include <picojson.h>
#include <sstream>

namespace ecs {
    const EventData &EventData::operator=(bool newBool) {
        type = EventDataType::Bool;
        b = newBool;
        return *this;
    }

    const EventData &EventData::operator=(int newInt) {
        type = EventDataType::Int;
        i = newInt;
        return *this;
    }

    const EventData &EventData::operator=(unsigned int newUint) {
        type = EventDataType::Uint;
        ui = newUint;
        return *this;
    }

    const EventData &EventData::operator=(float newFloat) {
        type = EventDataType::Float;
        f = newFloat;
        return *this;
    }

    const EventData &EventData::operator=(double newDouble) {
        type = EventDataType::Double;
        d = newDouble;
        return *this;
    }

    const EventData &EventData::operator=(glm::vec2 newVec2) {
        type = EventDataType::Vec2;
        vec2 = newVec2;
        return *this;
    }

    const EventData &EventData::operator=(glm::vec3 newVec3) {
        type = EventDataType::Vec3;
        vec3 = newVec3;
        return *this;
    }

    const EventData &EventData::operator=(glm::vec4 newVec4) {
        type = EventDataType::Vec4;
        vec4 = newVec4;
        return *this;
    }

    const EventData &EventData::operator=(Transform newTransform) {
        type = EventDataType::Transform;
        transform = newTransform;
        return *this;
    }

    const EventData &EventData::operator=(NamedEntity newNamedEntity) {
        type = EventDataType::NamedEntity;
        namedEntity = newNamedEntity;
        return *this;
    }

    const EventData &EventData::operator=(Entity newEntity) {
        type = EventDataType::Entity;
        ent = newEntity;
        return *this;
    }

    const EventData &EventData::operator=(EventString newString) {
        type = EventDataType::String;
        str = newString;
        return *this;
    }

    const EventData &EventData::operator=(EventBytes newBytes) {
        type = EventDataType::Bytes;
        bytes = newBytes;
        return *this;
    }

    bool EventData::operator==(const EventData &other) const {
        if (type != other.type) return false;
        return EventData::Visit(*this, [&other](auto &data) {
            using T = std::remove_cvref_t<decltype(data)>;
            return data == EventData::Get<T>(other);
        });
    }

    template<>
    bool StructMetadata::Load<EventData>(EventData &data, const picojson::value &src) {
        if (src.is<bool>()) {
            data = src.get<bool>();
        } else if (src.is<double>()) {
            data = src.get<double>();
        } else if (src.is<std::string>()) {
            auto &str = src.get<std::string>();
            if (str.starts_with("entity:")) {
                data = NamedEntity(Name(str.substr(7), EntityScope()));
            } else {
                data = str;
            }
        } else if (src.is<picojson::array>()) {
            auto &arr = src.get<picojson::array>();
            if (arr.size() == 2) {
                glm::vec2 vec(0);
                if (!sp::json::Load(vec, src)) {
                    Errorf("Unsupported EventData value: %s", src.to_str());
                    return false;
                }
                data = vec;
            } else if (arr.size() == 3) {
                glm::vec3 vec(0);
                if (!sp::json::Load(vec, src)) {
                    Errorf("Unsupported EventData value: %s", src.to_str());
                    return false;
                }
                data = vec;
            } else if (arr.size() == 4) {
                glm::vec4 vec(0);
                if (!sp::json::Load(vec, src)) {
                    Errorf("Unsupported EventData value: %s", src.to_str());
                    return false;
                }
                data = vec;
            } else {
                Errorf("Unsupported EventData array size: %u", arr.size());
                return false;
            }
        } else {
            Errorf("Unsupported EventData value: %s", src.to_str());
            return false;
        }
        return true;
    }

    template<>
    void StructMetadata::Save<EventData>(const EntityScope &scope,
        picojson::value &dst,
        const EventData &src,
        const EventData *def) {
        if (def && src == *def) return;

        EventData::Visit(src, [&](auto &data) {
            sp::json::Save(scope, dst, data);
        });
    }

    template<>
    void StructMetadata::SetScope<EventData>(EventData &dst, const EntityScope &scope) {
        if (dst.type == EventDataType::NamedEntity) {
            dst.namedEntity.SetScope(scope);
        }
    }

    size_t Event::Send(const DynamicLock<SendEventsLock> &lock, Entity target, const Event &event) {
        return EventBindings::SendEvent(lock, target, event);
    }

    size_t Event::SendNamed(const DynamicLock<SendEventsLock> &lock, const NamedEntity &target, const Event &event) {
        return EventBindings::SendEvent(lock, target, event);
    }

    size_t Event::SendRef(const DynamicLock<SendEventsLock> &lock, const EntityRef &target, const Event &event) {
        return EventBindings::SendEvent(lock, target, event);
    }

    std::string Event::ToString() const {
        std::stringstream ss;
        ss << std::string_view(this->name) << ":" << this->data;
        return ss.str();
    }

    std::ostream &operator<<(std::ostream &out, const EventData &v) {
        switch (v.type) {
        case EventDataType::Bool:
            out << "bool(" << v.b << ")";
            break;
        case EventDataType::Int:
            out << "int(" << v.i << ")";
            break;
        case EventDataType::Uint:
            out << "uint(" << v.ui << ")";
            break;
        case EventDataType::Float:
            out << "float(" << v.f << ")";
            break;
        case EventDataType::Double:
            out << "double(" << v.d << ")";
            break;
        case EventDataType::Vec2:
            out << glm::to_string(v.vec2);
            break;
        case EventDataType::Vec3:
            out << glm::to_string(v.vec3);
            break;
        case EventDataType::Vec4:
            out << glm::to_string(v.vec4);
            break;
        case EventDataType::Transform:
            out << glm::to_string(v.transform.offset) << " scale " << glm::to_string(v.transform.scale);
            break;
        case EventDataType::NamedEntity:
            out << v.namedEntity.Name().String();
            break;
        case EventDataType::Entity:
            out << std::to_string(v.ent);
            break;
        case EventDataType::String:
            out << "\"" << v.str << "\"";
            break;
        default:
            Abortf("Unexpected EventData type: %s", v.type);
        }
        return out;
    }

    bool EventQueue::Add(const AsyncEvent &event) {
        State s, s2;
        do {
            s = state.load();
            s2 = {s.head, (s.tail + 1) % (uint32_t)events.size()};
            if (s2.tail == s2.head) {
                EntityRef ref(event.source);
                Warnf("Event Queue full! Dropping event %s from %s", event.name, ref.Name().String());
                return false;
            }
        } while (!state.compare_exchange_weak(s, s2, std::memory_order_acquire, std::memory_order_relaxed));
        events[s.tail] = event;
        return true;
    }

    bool EventQueue::Add(const Event &event, size_t transactionId) {
        AsyncEvent asyncEvent(event.name, event.source, event.data);
        asyncEvent.transactionId = transactionId;
        return Add(asyncEvent);
    }

    bool EventQueue::Poll(Event &eventOut, size_t transactionId) {
        bool outputSet = false;
        while (!outputSet) {
            State s = state.load();
            if (s.head == s.tail) break;

            auto &async = events[s.head];
            // Check if this event should be visible to the current transaction.
            // Events are not visible to the transaction that emitted them.
            if (async.transactionId >= transactionId && transactionId > 0) break;
            if (!async.data || !async.data->Ready()) break;

            auto data = async.data->Get();
            if (data) {
                eventOut = Event{
                    async.name,
                    async.source,
                    *data,
                };
                outputSet = true;
            } else {
                // A null event means it was filtered out asynchronously, skip over it
            }

            State s2;
            do {
                s = state.load();
                s2 = {(s.head + 1) % (uint32_t)events.size(), s.tail};
            } while (!state.compare_exchange_weak(s, s2, std::memory_order_release, std::memory_order_relaxed));
        }
        if (!outputSet) eventOut = Event();
        return outputSet;
    }

    bool EventQueue::Empty() {
        State s = state.load();
        return s.head == s.tail;
    }

    size_t EventQueue::Size() {
        State s = state.load();
        if (s.head > s.tail) {
            return s.tail + events.size() - s.head;
        } else {
            return s.tail - s.head;
        }
    }

    EventQueueRef EventQueue::New(uint32_t maxQueueSize) {
        ZoneScoped;
        Assertf(maxQueueSize <= MAX_QUEUE_SIZE, "EventQueue size %u exceeds max size %u", maxQueueSize, MAX_QUEUE_SIZE);

        auto &ctx = GetECSContext();

        std::lock_guard lock(ctx.eventQueues.mutex);
        if (ctx.eventQueues.freeList.empty()) {
            size_t offset = ctx.eventQueues.pool.size();
#define QUEUE_POOL_BLOCK_SIZE 1
            ctx.eventQueues.pool.resize(offset + QUEUE_POOL_BLOCK_SIZE);
            for (size_t i = 0; i < QUEUE_POOL_BLOCK_SIZE; i++) {
                ctx.eventQueues.pool[offset + i].poolIndex = offset + i;
                ctx.eventQueues.freeList.push(offset + i);
            }
        }

        size_t freeIndex = ctx.eventQueues.freeList.top();
        ctx.eventQueues.freeList.pop();
        EventQueue *newQueue = &ctx.eventQueues.pool[freeIndex];
        newQueue->events.resize(maxQueueSize);
        newQueue->state.store({0, 0});
        return std::shared_ptr<EventQueue>(newQueue, [&ctx](EventQueue *queuePtr) {
            if (queuePtr) {
                Assertf(ctx.eventQueues.pool.size() > 0, "EventQueuePool destroyed before EventQueueRef");
                queuePtr->state.store({0, 0});
                queuePtr->events.resize(0);
                std::lock_guard lock(ctx.eventQueues.mutex);
                if (queuePtr->poolIndex < ctx.eventQueues.pool.size()) {
                    ctx.eventQueues.freeList.push(queuePtr->poolIndex);
                }
            }
        });
    }
} // namespace ecs
