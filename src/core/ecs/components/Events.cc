#include "Events.hh"

namespace ecs {
    bool EventInput::Add(const std::string binding, const Event &event) {
        auto queue = events.find(binding);
        if (queue != events.end()) {
            queue->second.emplace(event);
            return true;
        }
        return false;
    }

    bool EventInput::Poll(const std::string binding, Event &eventOut) {
        auto queue = events.find(binding);
        if (queue != events.end() && !queue->second.empty()) {
            eventOut = queue->second.front();
            queue->second.pop();
            return true;
        }
        eventOut = Event();
        return false;
    }

    void EventBindings::Bind(std::string source, Tecs::Entity target, std::string dest) {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            list->second.emplace_back(target, dest);
        } else {
            sourceToDest.emplace(source, std::initializer_list<Binding>{Binding(target, dest)});
        }
    }

    void EventBindings::Unbind(std::string source, Tecs::Entity target, std::string dest) {
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

    void EventBindings::UnbindTarget(Tecs::Entity target) {
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

    void EventBindings::UnbindDest(Tecs::Entity target, std::string dest) {
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

    const EventBindings::BindingList *EventBindings::Lookup(std::string source) const {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) { return &list->second; }
        return nullptr;
    }

    void EventBindings::SendEvent(Lock<Write<EventInput>> lock, const Event &event) const {
        auto list = sourceToDest.find(event.name);
        if (list != sourceToDest.end()) {
            for (auto &binding : list->second) {
                auto &eventInput = binding.first.Get<EventInput>(lock);
                eventInput.Add(binding.second, event);
            }
        }
    }
} // namespace ecs
