#include "Events.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EntityReferenceManager.hh"

#include <optional>
#include <picojson/picojson.h>
#include <sstream>

namespace ecs {

    template<>
    bool StructMetadata::Load<EventDest>(const EntityScope &scope, EventDest &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto [targetName, eventName] = ParseEventString(src.get<std::string>(), scope);
        if (targetName) {
            dst.target = targetName;
            dst.queueName = eventName;
            return true;
        } else {
            return false;
        }
    }

    template<>
    void StructMetadata::Save<EventDest>(const EntityScope &scope,
        picojson::value &dst,
        const EventDest &src,
        const EventDest &def) {
        dst = picojson::value(src.target.Name().String() + src.queueName);
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

    template<>
    bool StructMetadata::Load<EventBindingActions>(const EntityScope &scope,
        EventBindingActions &dst,
        const picojson::value &src) {
        if (src.is<picojson::object>()) {
            auto &obj = src.get<picojson::object>();
            if (obj.count("set_value") > 0) {
                Event::EventData eventData;
                if (parseEventData(eventData, obj.at("set_value"))) {
                    dst.setValue = eventData;
                } else {
                    return false;
                }
            }
        }
        return true;
    }
    template<>
    void StructMetadata::Save<EventBindingActions>(const EntityScope &scope,
        picojson::value &dst,
        const EventBindingActions &src,
        const EventBindingActions &def) {
        if (src.setValue) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            auto &obj = dst.get<picojson::object>();
            std::visit(
                [&](auto &&value) {
                    sp::json::Save(scope, obj["set_value"], value);
                },
                *src.setValue);
        }
    }

    template<>
    bool StructMetadata::Load<EventBinding>(const EntityScope &scope,
        EventBinding &binding,
        const picojson::value &src) {
        if (src.is<std::string>()) {
            if (!sp::json::Load(scope, binding.outputs, src)) {
                Errorf("Invalid event binding output: %s", src.get<std::string>());
                return false;
            }
        } else if (!src.is<picojson::object>()) {
            Errorf("Unknown event binding type: %s", src.to_str());
            return false;
        }
        return true;
    }

    template<>
    void StructMetadata::Save<EventBinding>(const EntityScope &scope,
        picojson::value &dst,
        const EventBinding &src,
        const EventBinding &def) {
        if (src.actions == def.actions) {
            sp::json::SaveIfChanged(scope, dst, "", src.outputs, def.outputs);
        }
    }

    template<>
    bool StructMetadata::Load<EventBindings>(const EntityScope &scope, EventBindings &dst, const picojson::value &src) {
        if (!src.is<picojson::object>()) {
            Errorf("Invalid event bindings: %s", src.to_str());
            return false;
        }
        for (auto &param : src.get<picojson::object>()) {
            if (param.second.is<std::string>()) {
                EventBinding binding;
                if (!sp::json::Load(scope, binding, param.second)) {
                    Errorf("Invalid event binding: %s", param.second.get<std::string>());
                    return false;
                }
                dst.Bind(param.first, binding);
            } else if (param.second.is<picojson::object>()) {
                EventBinding binding;
                if (!sp::json::Load(scope, binding, param.second)) {
                    Errorf("Invalid event binding: %s", src.to_str());
                    return false;
                }
                dst.Bind(param.first, binding);
            } else if (param.second.is<picojson::array>()) {
                for (auto &entry : param.second.get<picojson::array>()) {
                    EventBinding binding;
                    if (!sp::json::Load(scope, binding, entry)) {
                        Errorf("Invalid event binding: %s", src.to_str());
                        return false;
                    }
                    dst.Bind(param.first, binding);
                }
            } else {
                Errorf("Unknown event binding type: %s", src.to_str());
                return false;
            }
        }
        return true;
    }

    template<>
    void Component<EventBindings>::Apply(EventBindings &dst, const EventBindings &src, bool liveTarget) {
        for (auto &[source, srcList] : src.sourceToDest) {
            for (auto &binding : srcList) {
                dst.Bind(source, binding);
            }
        }
    }

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str, const EntityScope &scope) {
        size_t delimiter = str.find('/');
        ecs::Name entityName(str.substr(0, delimiter), scope);
        if (entityName && delimiter != std::string::npos) {
            return std::make_pair(entityName, str.substr(delimiter));
        } else {
            return std::make_pair(entityName, "");
        }
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

    size_t EventInput::Add(const Event &event) const {
        size_t eventsSent = 0;
        auto it = events.find(event.name);
        if (it != events.end()) {
            for (auto &queue : it->second) {
                queue->Add(event);
                eventsSent++;
            }
        }
        return eventsSent;
    }

    bool EventInput::Poll(Lock<Read<EventInput>> lock, const EventQueueRef &queue, Event &eventOut) {
        if (!queue) return false;
        return queue->Poll(eventOut);
    }

    EventBinding &EventBindings::Bind(std::string source, const EventBinding &binding) {
        auto &list = sourceToDest.emplace(source, BindingList{}).first->second;
        auto it = std::find_if(list.begin(), list.end(), [&](auto &arg) {
            return arg.actions == binding.actions;
        });
        if (it == list.end()) {
            // No matching bindings exist, add a new one
            return list.emplace_back(binding);
        } else {
            // A binding with the same settings exists, append to its output list
            for (auto &output : binding.outputs) {
                if (!sp::contains(it->outputs, output)) it->outputs.emplace_back(output);
            }
            return *it;
        }
    }

    EventBinding &EventBindings::Bind(std::string source, EntityRef target, std::string dest) {
        EventBinding binding;
        binding.outputs = {EventDest{target, dest}};
        return Bind(source, binding);
    }

    void EventBindings::Unbind(std::string source, EntityRef target, std::string dest) {
        auto list = sourceToDest.find(source);
        if (list != sourceToDest.end()) {
            EventDest searchDest = {target, dest};
            auto binding = list->second.begin();
            while (binding != list->second.end()) {
                if (sp::contains(binding->outputs, searchDest)) {
                    if (binding->outputs.size() == 1) {
                        binding = list->second.erase(binding);
                    } else {
                        sp::erase(binding->outputs, searchDest);
                        binding++;
                    }
                } else {
                    binding++;
                }
            }
        }
    }

    void modifyEvent(ecs::ReadSignalsLock lock,
        Event &event,
        const Event::EventData &input,
        const EventBinding &binding,
        size_t depth) {
        auto &actions = binding.actions.modifyExprs;
        std::visit(
            [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (sp::is_glm_vec<T>()) {
                    using U = typename T::value_type;
                    if (actions.size() != (size_t)T::length()) {
                        Errorf("Event binding modify value is wrong size: %u != %u", actions.size(), T::length());
                        return;
                    }
                    for (int i = 0; i < T::length(); i++) {
                        if constexpr (std::is_same_v<U, bool>) {
                            arg[i] = actions[i].EvaluateEvent(lock, input, depth) >= 0.5;
                        } else {
                            arg[i] = (U)actions[i].EvaluateEvent(lock, input, depth);
                        }
                    }
                } else if constexpr (std::is_same_v<T, bool>) {
                    if (actions.size() != 1) {
                        Errorf("Event binding modify value is wrong size: %u != 1", actions.size());
                        return;
                    }
                    arg = actions[0].EvaluateEvent(lock, input, depth) >= 0.5;
                } else if constexpr (std::is_convertible_v<double, T>) {
                    if (actions.size() != 1) {
                        Errorf("Event binding modify value is wrong size: %u != 1", actions.size());
                        return;
                    }
                    arg = (T)actions[0].EvaluateEvent(lock, input, depth);
                } else {
                    Errorf("Unsupported event binding modify value: type: %s vec%u", typeid(T).name(), actions.size());
                }
            },
            event.data);
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

        size_t eventsSent = 0;
        if (ent.Has<EventInput>(lock)) {
            auto &eventInput = ent.Get<EventInput>(lock);
            eventsSent += eventInput.Add(event);
        }
        if (ent.Has<EventBindings>(lock)) {
            auto &bindings = ent.Get<EventBindings>(lock);
            auto list = bindings.sourceToDest.find(event.name);
            if (list != bindings.sourceToDest.end()) {
                for (auto &binding : list->second) {
                    // Execute event modifiers before submitting to the destination queue
                    if (binding.actions.filterExpr) {
                        ecs::QueueTransaction<ecs::SendEventsLock, ecs::ReadSignalsLock>(
                            [event, binding, depth](auto lock) {
                                if (binding.actions.filterExpr->EvaluateEvent(lock, event.data) < 0.5) return;

                                Event outputEvent = event;
                                if (binding.actions.setValue) {
                                    outputEvent.data = *binding.actions.setValue;
                                }
                                if (!binding.actions.modifyExprs.empty()) {
                                    modifyEvent(lock, outputEvent, event.data, binding, depth);
                                }
                                for (auto &dest : binding.outputs) {
                                    outputEvent.name = dest.queueName;
                                    EventBindings::SendEvent(lock, dest.target, outputEvent, depth + 1);
                                }
                            });
                        continue;
                    }
                    Event modifiedEvent = event;
                    if (binding.actions.setValue) {
                        modifiedEvent.data = *binding.actions.setValue;
                    }
                    if (!binding.actions.modifyExprs.empty()) {
                        ecs::QueueTransaction<ecs::SendEventsLock, ecs::ReadSignalsLock>(
                            [modifiedEvent, event, binding, depth](auto lock) {
                                Event outputEvent = modifiedEvent;
                                modifyEvent(lock, outputEvent, event.data, binding, depth);
                                for (auto &dest : binding.outputs) {
                                    outputEvent.name = dest.queueName;
                                    EventBindings::SendEvent(lock, dest.target, outputEvent, depth + 1);
                                }
                            });
                        continue;
                    }
                    for (auto &dest : binding.outputs) {
                        modifiedEvent.name = dest.queueName;
                        eventsSent += EventBindings::SendEvent(lock, dest.target, modifiedEvent, depth + 1);
                    }
                }
            }
        }
        return eventsSent;
    }
} // namespace ecs
