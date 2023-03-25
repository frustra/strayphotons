#include "EventQueue.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EntityReferenceManager.hh"

#include <optional>
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
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

    EventQueueRef NewEventQueue(uint32_t maxQueueSize) {
        return make_shared<EventQueue>(maxQueueSize);
    }
} // namespace ecs
