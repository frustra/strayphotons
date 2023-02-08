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

    std::ostream &operator<<(std::ostream &out, const Event::EventData &v) {
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

    void EventQueue::Add(const Event &event) {
        std::lock_guard lock(mutex);
        events.emplace(event);
    }

    bool EventQueue::Empty() {
        std::lock_guard lock(mutex);
        return events.empty();
    }

    size_t EventQueue::Size() {
        std::lock_guard lock(mutex);
        return events.size();
    }

    bool EventQueue::Poll(Event &eventOut) {
        std::lock_guard lock(mutex);
        if (events.empty()) {
            eventOut = Event();
            return false;
        }

        eventOut = events.front();
        events.pop();
        return true;
    }

    EventQueueRef NewEventQueue() {
        return make_shared<EventQueue>();
    }
} // namespace ecs
