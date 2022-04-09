#include "Events.hh"

#include "core/Logging.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"

#include <optional>
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {
    template<>
    bool Component<EventInput>::Load(ScenePtr scenePtr,
        const Name &scope,
        EventInput &input,
        const picojson::value &src) {
        for (auto event : src.get<picojson::array>()) {
            input.Register(event.get<std::string>());
        }
        return true;
    }

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

    bool parseEventBinding(const Name &scope, EventBindings::Binding &binding, const picojson::value &src) {
        if (src.is<std::string>()) {
            auto [targetName, eventName] = ParseEventString(src.get<std::string>(), scope);
            if (targetName) {
                binding.target = GEntityRefs.Get(targetName);
                binding.destQueue = eventName;
            } else {
                Errorf("Invalid event binding target: %s", src.get<std::string>());
                return false;
            }
        } else if (src.is<picojson::object>()) {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "target") {
                    if (param.second.is<std::string>()) {
                        auto [targetName, eventName] = ParseEventString(param.second.get<std::string>(), scope);
                        if (targetName) {
                            binding.target = GEntityRefs.Get(targetName);
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
    bool Component<EventBindings>::Load(ScenePtr scenePtr,
        const Name &scope,
        EventBindings &bindings,
        const picojson::value &src) {
        auto scene = scenePtr.lock();
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
    void Component<EventInput>::Apply(const EventInput &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstInput = dst.Get<EventInput>(lock);
        for (auto &input : src.events) {
            if (!dstInput.IsRegistered(input.first)) dstInput.Register(input.first);
        }
    }

    template<>
    void Component<EventBindings>::Apply(const EventBindings &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstBindings = dst.Get<EventBindings>(lock);
        dstBindings.CopyBindings(src);
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
        ecs::Name entityName;
        if (entityName.Parse(str.substr(0, delimiter), scope)) {
            if (delimiter != std::string::npos) {
                return std::make_pair(entityName, str.substr(delimiter));
            } else {
                return std::make_pair(entityName, "");
            }
        } else {
            return std::make_pair(ecs::Name(), "");
        }
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

    bool EventInput::Poll(Lock<Write<EventInput>> lock, Entity ent, const std::string &binding, Event &eventOut) {
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

    void EventBindings::Bind(std::string source, const Binding &binding) {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            auto &vec = list->second;
            if (std::find(vec.begin(), vec.end(), binding) == vec.end()) vec.emplace_back(binding);
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
                auto ent = binding.target.Get();
                if (focusLock && ent.Has<FocusLayer>(lock)) {
                    auto &layer = ent.Get<FocusLayer>(lock);
                    if (!focusLock->HasPrimaryFocus(layer)) continue;
                }
                if (ent.Exists(lock)) {
                    if (ent.Has<EventInput>(lock)) {
                        auto &eventInput = ent.Get<EventInput>(lock);

                        // Execute event modifiers before submitting to the destination queue
                        if (binding.setValue) {
                            Event modifiedEvent = event;
                            modifiedEvent.data = *binding.setValue;
                            eventInput.Add(binding.destQueue, modifiedEvent);
                        } else {
                            eventInput.Add(binding.destQueue, event);
                        }
                    } else {
                        Errorf("Tried to send event to entity without EventInput: %s", binding.target.Name().String());
                    }
                } else {
                    Errorf("Tried to send event to missing entity: %s", binding.target.Name().String());
                }
            }
        }
    }

    std::vector<std::string> EventBindings::GetBindingNames() const {
        std::vector<std::string> list(sourceToDest.size());
        size_t i = 0;
        for (auto &entry : sourceToDest) {
            list[i++] = entry.first;
        }
        return list;
    }
} // namespace ecs
