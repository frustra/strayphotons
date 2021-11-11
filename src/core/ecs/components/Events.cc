#include "Events.hh"

#include "core/Logging.hh"

#include <optional>
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    template<>
    bool Component<EventInput>::Load(sp::Scene *scene, EventInput &input, const picojson::value &src) {
        for (auto event : src.get<picojson::array>()) {
            input.Register(event.get<std::string>());
        }
        return true;
    }

    template<>
    bool Component<EventBindings>::Load(sp::Scene *scene, EventBindings &bindings, const picojson::value &src) {
        for (auto bind : src.get<picojson::object>()) {
            for (auto dest : bind.second.get<picojson::object>()) {
                picojson::array targetList;
                if (dest.second.is<std::string>()) {
                    targetList.emplace_back(dest.second);
                } else if (dest.second.is<picojson::array>()) {
                    targetList = dest.second.get<picojson::array>();
                } else {
                    sp::Abort("Invalid event target");
                }
                for (auto target : targetList) {
                    auto targetName = target.get<std::string>();
                    bindings.Bind(bind.first, NamedEntity(targetName), dest.first);
                }
            }
        }
        return true;
    }

    std::string Event::toString() const {
        std::stringstream ss;
        ss << this->data;
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
                } else if constexpr (std::is_same_v<T, Tecs::Entity>) {
                    out << "Entity(" << arg.id << ")";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    out << "\"" << arg << "\"";
                } else {
                    out << typeid(arg).name() << "(" << arg << ")";
                }
            },
            v);
        return out;
    }

    void EventInput::Register(const std::string &binding) {
        events.emplace(binding, std::queue<Event>());
    }

    bool EventInput::IsRegistered(const std::string &binding) const {
        return events.count(binding) > 0;
    }

    void EventInput::Unregister(const std::string &binding) {
        events.erase(binding);
    }

    bool EventInput::Add(const std::string &binding, const Event &event) {
        auto queue = events.find(binding);
        if (queue != events.end()) {
            queue->second.emplace(event);
            return true;
        }
        return false;
    }

    bool EventInput::HasEvents(const std::string &binding) const {
        auto queue = events.find(binding);
        return queue != events.end() && !queue->second.empty();
    }

    bool EventInput::Poll(const std::string &binding, Event &eventOut) {
        auto queue = events.find(binding);
        if (queue != events.end() && !queue->second.empty()) {
            eventOut = queue->second.front();
            queue->second.pop();
            return true;
        }
        eventOut = Event();
        return false;
    }

    bool EventInput::Poll(Lock<Write<EventInput>> lock, Tecs::Entity ent, const std::string &binding, Event &eventOut) {
        auto &readInput = ent.Get<const EventInput>(lock);
        if (readInput.HasEvents(binding)) {
            auto &writeInput = ent.Get<EventInput>(lock);
            return writeInput.Poll(binding, eventOut);
        }
        eventOut = Event();
        return false;
    }

    void EventBindings::CopyBindings(const EventBindings &src) {
        for (auto &[source, srcList] : src.sourceToDest) {
            auto dstList = sourceToDest.find(source);
            if (dstList != sourceToDest.end()) {
                auto &vec = dstList->second;
                for (auto &binding : srcList) {
                    if (std::find(vec.begin(), vec.end(), binding) == vec.end()) vec.emplace_back(binding);
                }
            } else {
                sourceToDest.emplace(source, srcList);
            }
        }
    }

    void EventBindings::Bind(std::string source, NamedEntity target, std::string dest) {
        Binding newBinding(target, dest);

        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            auto &vec = list->second;
            if (std::find(vec.begin(), vec.end(), newBinding) == vec.end()) vec.emplace_back(newBinding);
        } else {
            sourceToDest.emplace(source, BindingList{newBinding});
        }
    }

    void EventBindings::Unbind(std::string source, NamedEntity target, std::string dest) {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            auto binding = list->second.begin();
            while (binding != list->second.end()) {
                if (binding->first == target && binding->second == dest) {
                    binding = list->second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void EventBindings::UnbindSource(std::string source) {
        sourceToDest.erase(source);
    }

    void EventBindings::UnbindTarget(NamedEntity target) {
        for (auto &list : sourceToDest) {
            auto binding = list.second.begin();
            while (binding != list.second.end()) {
                if (binding->first == target) {
                    binding = list.second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void EventBindings::UnbindDest(NamedEntity target, std::string dest) {
        for (auto &list : sourceToDest) {
            auto binding = list.second.begin();
            while (binding != list.second.end()) {
                if (binding->first == target && binding->second == dest) {
                    binding = list.second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    const EventBindings::BindingList *EventBindings::Lookup(const std::string source) const {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) { return &list->second; }
        return nullptr;
    }

    void EventBindings::SendEvent(Lock<Read<Name, FocusLayer, FocusLock>, Write<EventInput>> lock,
        const Event &event) const {
        const FocusLock *focusLock = nullptr;
        if (lock.Has<FocusLock>()) focusLock = &lock.Get<FocusLock>();
        auto list = sourceToDest.find(event.name);
        if (list != sourceToDest.end()) {
            for (auto &binding : list->second) {
                auto ent = binding.first.Get(lock);
                if (focusLock && ent.Has<FocusLayer>(lock)) {
                    auto &layer = ent.Get<FocusLayer>(lock);
                    if (!focusLock->HasPrimaryFocus(layer)) continue;
                }
                if (ent.Has<EventInput>(lock)) {
                    auto &eventInput = ent.Get<EventInput>(lock);
                    eventInput.Add(binding.second, event);
                } else {
                    Errorf("Tried to send event to missing entity: %s", binding.first.Name());
                }
            }
        }
    }
} // namespace ecs
