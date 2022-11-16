#include "Events.hh"

#include "core/Logging.hh"
#include "ecs/EntityReferenceManager.hh"

#include <optional>
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    bool parseEventData(Event::EventData &data, const picojson::value &src) {
        if (src.is<bool>()) {
            data = src.get<bool>();
        } else if (src.is<double>()) {
            data = src.get<double>();
        } else if (src.is<std::string>()) {
            data = src.get<std::string>();
        } else {
            Errorf("Unsupported EventData value: %s", src.to_str());
            return false;
        }
        return true;
    }

    bool parseEventBinding(const EntityScope &scope, EventBindings::Binding &binding, const picojson::value &src) {
        if (src.is<std::string>()) {
            auto [targetName, eventName] = ParseEventString(src.get<std::string>(), scope.prefix);
            if (targetName) {
                binding.target = targetName;
                binding.destQueue = eventName;
            } else {
                Errorf("Invalid event binding target: %s", src.get<std::string>());
                return false;
            }
        } else if (src.is<picojson::object>()) {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "target") {
                    if (param.second.is<std::string>()) {
                        auto [targetName, eventName] = ParseEventString(param.second.get<std::string>(), scope.prefix);
                        if (targetName) {
                            binding.target = targetName;
                            binding.destQueue = eventName;
                        } else {
                            Errorf("Invalid event binding target: %s", param.second.get<std::string>());
                            return false;
                        }
                    } else {
                        Errorf("Invalid event binding target type: %s", param.second.to_str());
                        return false;
                    }
                } else if (param.first == "set_value") {
                    Event::EventData eventData;
                    if (parseEventData(eventData, param.second)) {
                        binding.setValue = eventData;
                    } else {
                        Errorf("Invalid event binding set_value: %s", param.second.to_str());
                        return false;
                    }
                } else if (param.first == "multiply_value") {
                    if (param.second.is<double>()) {
                        binding.multiplyValue = param.second.get<double>();
                    } else {
                        Errorf("Unsupported multiply_value value: %s", param.second.to_str());
                        return false;
                    }
                } else {
                    Errorf("Unknown event binding field: %s", param.first);
                    return false;
                }
            }
        } else {
            Errorf("Unknown event binding type: %s", src.to_str());
            return false;
        }
        return true;
    }

    template<>
    bool Component<EventBindings>::Load(const EntityScope &scope, EventBindings &bindings, const picojson::value &src) {
        auto scene = scope.scene.lock();
        Assert(scene, "EventBindings::Load must have valid scene");
        for (auto param : src.get<picojson::object>()) {
            if (param.second.is<picojson::array>()) {
                for (auto bind : param.second.get<picojson::array>()) {
                    EventBindings::Binding binding;
                    if (!parseEventBinding(scope, binding, bind)) return false;
                    bindings.Bind(param.first, binding);
                }
            } else {
                EventBindings::Binding binding;
                if (!parseEventBinding(scope, binding, param.second)) return false;
                bindings.Bind(param.first, binding);
            }
        }
        return true;
    }

    template<>
    void Component<EventBindings>::Apply(const EventBindings &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstBindings = dst.Get<EventBindings>(lock);
        dstBindings.CopyBindings(src);
    }

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
                    out << glm::to_string(arg.matrix);
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

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str, const Name &scope) {
        size_t delimiter = str.find('/');
        ecs::Name entityName(str.substr(0, delimiter), scope);
        if (entityName && delimiter != std::string::npos) {
            return std::make_pair(entityName, str.substr(delimiter));
        } else {
            return std::make_pair(entityName, "");
        }
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

    void EventInput::Register(Lock<Write<EventInput>> lock, const EventQueueRef &queue, const std::string &binding) {
        Assertf(IsLive(lock), "Attempting to register event on non-live entity: %s", binding);
        Assertf(queue, "EventInput::Register called with null queue: %s", binding);

        auto &queueList = events[binding];
        if (sp::contains(queueList, queue)) return;
        queueList.emplace_back(queue);
    }

    void EventInput::Unregister(const std::shared_ptr<EventQueue> &queue, const std::string &binding) {
        if (!queue) return;

        auto it = events.find(binding);
        if (it != events.end()) {
            sp::erase(it->second, queue);
            if (it->second.empty()) events.erase(it);
        }
    }

    bool EventInput::Add(const Event &event) const {
        auto it = events.find(event.name);
        if (it != events.end()) {
            for (auto &queue : it->second) {
                queue->Add(event);
            }
            return true;
        }
        return false;
    }

    bool EventInput::Poll(Lock<Read<EventInput>> lock, const EventQueueRef &queue, Event &eventOut) {
        if (!queue) return false;
        return queue->Poll(eventOut);
    }

    void EventBindings::CopyBindings(const EventBindings &src) {
        for (auto &[source, srcList] : src.sourceToDest) {
            auto dstList = sourceToDest.find(source);
            if (dstList != sourceToDest.end()) {
                auto &vec = dstList->second;
                for (auto &binding : srcList) {
                    if (!sp::contains(vec, binding)) vec.emplace_back(binding);
                }
            } else {
                sourceToDest.emplace(source, srcList);
            }
        }
    }

    void EventBindings::Bind(std::string source, const Binding &binding) {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            auto &vec = list->second;
            if (!sp::contains(vec, binding)) vec.emplace_back(binding);
        } else {
            sourceToDest.emplace(source, BindingList{binding});
        }
    }

    void EventBindings::Bind(std::string source, EntityRef target, std::string dest) {
        Binding binding;
        binding.target = target;
        binding.destQueue = dest;
        Bind(source, binding);
    }

    void EventBindings::Unbind(std::string source, EntityRef target, std::string dest) {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            auto binding = list->second.begin();
            while (binding != list->second.end()) {
                if (binding->target == target && binding->destQueue == dest) {
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

    void EventBindings::UnbindTarget(EntityRef target) {
        for (auto &list : sourceToDest) {
            auto binding = list.second.begin();
            while (binding != list.second.end()) {
                if (binding->target == target) {
                    binding = list.second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void EventBindings::UnbindDest(EntityRef target, std::string dest) {
        for (auto &list : sourceToDest) {
            auto binding = list.second.begin();
            while (binding != list.second.end()) {
                if (binding->target == target && binding->destQueue == dest) {
                    binding = list.second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    const EventBindings::BindingList *EventBindings::Lookup(const std::string source) const {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            return &list->second;
        }
        return nullptr;
    }

    std::vector<std::string> EventBindings::GetBindingNames() const {
        std::vector<std::string> list(sourceToDest.size());
        size_t i = 0;
        for (auto &entry : sourceToDest) {
            list[i++] = entry.first;
        }
        return list;
    }

    size_t EventBindings::SendEvent(SendEventsLock lock, const EntityRef &target, const Event &event, size_t depth) {
        if (depth > MAX_EVENT_BINDING_DEPTH) {
            Errorf("Max event binding depth exceeded: %s %s", target.Name().String(), event.name);
            return 0;
        }
        auto ent = target.Get(lock);
        if (!ent.Exists(lock)) {
            Errorf("Tried to send event to missing entity: %s", target.Name().String());
            return 0;
        }

        const FocusLock *focusLock = nullptr;
        if (lock.Has<FocusLock>()) focusLock = &lock.Get<FocusLock>();
        if (focusLock && ent.Has<FocusLayer>(lock)) {
            auto &layer = ent.Get<FocusLayer>(lock);
            if (!focusLock->HasPrimaryFocus(layer)) return 0;
        }

        size_t eventsSent = 0;
        if (ent.Has<EventInput>(lock)) {
            auto &eventInput = ent.Get<EventInput>(lock);
            if (eventInput.Add(event)) eventsSent++;
        }
        if (ent.Has<EventBindings>(lock)) {
            auto &bindings = ent.Get<EventBindings>(lock);
            auto list = bindings.sourceToDest.find(event.name);
            if (list != bindings.sourceToDest.end()) {
                for (auto &binding : list->second) {
                    // Execute event modifiers before submitting to the destination queue
                    Event modifiedEvent = event;
                    modifiedEvent.name = binding.destQueue;
                    if (binding.setValue) {
                        modifiedEvent.data = *binding.setValue;
                    } else if (binding.multiplyValue) {
                        std::visit(
                            [&](auto &&arg) {
                                using T = std::decay_t<decltype(arg)>;
                                if constexpr (std::is_same_v<T, glm::vec2>) {
                                    arg *= *binding.multiplyValue;
                                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                                    arg *= *binding.multiplyValue;
                                } else if constexpr (std::is_same_v<T, bool>) {
                                    modifiedEvent.data = ((double)arg) * (*binding.multiplyValue);
                                } else if constexpr (std::is_same_v<T, int>) {
                                    modifiedEvent.data = ((double)arg) * (*binding.multiplyValue);
                                } else if constexpr (std::is_same_v<T, double>) {
                                    arg *= *binding.multiplyValue;
                                } else {
                                    Abortf("Unsupported event type for multiply_value: %s", typeid(arg).name());
                                }
                            },
                            modifiedEvent.data);
                    }
                    eventsSent += EventBindings::SendEvent(lock, binding.target, modifiedEvent, depth + 1);
                }
            }
        }
        return eventsSent;
    }
} // namespace ecs
