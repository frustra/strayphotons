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
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    template<>
    bool StructMetadata::Load<EventData>(EventData &data, const picojson::value &src) {
        if (src.is<bool>()) {
            data = src.get<bool>();
        } else if (src.is<double>()) {
            data = src.get<double>();
        } else if (src.is<std::string>()) {
            data = src.get<std::string>();
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

        std::visit(
            [&](auto &&value) {
                sp::json::Save(scope, dst, value);
            },
            src);
    }

    std::string Event::toString() const {
        std::stringstream ss;
        ss << this->name << ":" << this->data;
        return ss.str();
    }

    std::ostream &operator<<(std::ostream &out, const EventData &v) {
        std::visit(
            [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, glm::vec2>) {
                    out << glm::to_string(arg);
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    out << glm::to_string(arg);
                } else if constexpr (std::is_same_v<T, Transform>) {
                    out << glm::to_string(arg.offset) << " scale " << glm::to_string(arg.scale);
                } else if constexpr (std::is_same_v<T, EntityRef>) {
                    out << arg.Name().String();
                } else if constexpr (std::is_same_v<T, Tecs::Entity>) {
                    out << std::to_string(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    out << "\"" << arg << "\"";
                } else {
                    out << typeid(arg).name() << "(" << arg << ")";
                }
            },
            v);
        return out;
    }

    bool EventQueue::Add(const AsyncEvent &event) {
        State s, s2;
        do {
            s = state.load();
            s2 = {s.head, (s.tail + 1) % (uint32_t)events.size()};
            if (s2.tail == s2.head) {
                Warnf("Event Queue full! Dropping event %s from %s", event.name, std::to_string(event.source));
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
