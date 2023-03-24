#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalExpression.hh"

#include <optional>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_EVENT_BINDING_DEPTH = 10;

    using SendEventsLock = Lock<Read<Name, FocusLock, EventBindings, EventInput, SignalBindings, SignalOutput>>;

    struct EventInput {
        EventInput() {}

        void Register(Lock<Write<EventInput>> lock, const EventQueueRef &queue, const std::string &binding);
        void Unregister(const EventQueueRef &queue, const std::string &binding);

        size_t Add(const Event &event) const;
        static bool Poll(Lock<Read<EventInput>> lock, const EventQueueRef &queue, Event &eventOut);

        robin_hood::unordered_map<std::string, std::vector<std::shared_ptr<EventQueue>>> events;
    };

    static StructMetadata MetadataEventInput(typeid(EventInput));
    static Component<EventInput> ComponentEventInput("event_input", MetadataEventInput);

    struct EventDest {
        EntityRef target;
        std::string queueName;

        bool operator==(const EventDest &) const = default;
    };

    static StructMetadata MetadataEventDest(typeid(EventDest));
    template<>
    bool StructMetadata::Load<EventDest>(const EntityScope &scope, EventDest &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventDest>(const EntityScope &scope,
        picojson::value &dst,
        const EventDest &src,
        const EventDest &def);

    struct EventBindingActions {
        std::optional<SignalExpression> filterExpr;
        std::vector<SignalExpression> modifyExprs;
        std::optional<Event::EventData> setValue;

        bool operator==(const EventBindingActions &) const = default;
    };

    static StructMetadata MetadataEventBindingActions(typeid(EventBindingActions),
        StructField::New("filter", &EventBindingActions::filterExpr),
        StructField::New("modify", &EventBindingActions::modifyExprs));
    template<>
    bool StructMetadata::Load<EventBindingActions>(const EntityScope &scope,
        EventBindingActions &dst,
        const picojson::value &src);
    template<>
    void StructMetadata::Save<EventBindingActions>(const EntityScope &scope,
        picojson::value &dst,
        const EventBindingActions &src,
        const EventBindingActions &def);

    struct EventBinding {
        std::vector<EventDest> outputs;

        EventBindingActions actions;

        bool operator==(const EventBinding &) const = default;
    };

    static StructMetadata MetadataEventBinding(typeid(EventBinding),
        StructField::New("outputs", &EventBinding::outputs),
        StructField::New(&EventBinding::actions));
    template<>
    bool StructMetadata::Load<EventBinding>(const EntityScope &scope, EventBinding &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventBinding>(const EntityScope &scope,
        picojson::value &dst,
        const EventBinding &src,
        const EventBinding &def);

    class EventBindings {
    public:
        EventBindings() {}

        EventBinding &Bind(std::string source, const EventBinding &binding);
        EventBinding &Bind(std::string source, EntityRef target, std::string dest);
        void Unbind(std::string source, EntityRef target, std::string dest);

        template<typename LockType>
        static size_t SendEvent(LockType lock, const EntityRef &target, const Event &event, size_t depth = 0);

        using BindingList = typename std::vector<EventBinding>;
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    static StructMetadata MetadataEventBindings(typeid(EventBindings),
        StructField::New(&EventBindings::sourceToDest, FieldAction::AutoSave));
    static Component<EventBindings> ComponentEventBindings("event_bindings", MetadataEventBindings);
    template<>
    bool StructMetadata::Load<EventBindings>(const EntityScope &scope, EventBindings &dst, const picojson::value &src);
    template<>
    void Component<EventBindings>::Apply(EventBindings &dst, const EventBindings &src, bool liveTarget);

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str, const EntityScope &scope = Name());

    namespace detail {
        void ModifyEvent(ReadSignalsLock lock,
            Event &event,
            const Event::EventData &input,
            const EventBinding &binding,
            size_t depth);
    }

    template<typename LockType>
    size_t EventBindings::SendEvent(LockType lock, const EntityRef &target, const Event &event, size_t depth) {
        if (depth > MAX_EVENT_BINDING_DEPTH) {
            Errorf("Max event binding depth exceeded: %s %s", target.Name().String(), event.name);
            return 0;
        }
        Entity ent = target.Get(lock);
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
                        if constexpr (LockType::template has_permissions<ReadSignalsLock>()) {
                            if (binding.actions.filterExpr->EvaluateEvent(lock, event.data) < 0.5) continue;
                        } else {
                            ecs::QueueTransaction<ReadSignalsLock>([event, binding, depth](auto lock) {
                                if (binding.actions.filterExpr->EvaluateEvent(lock, event.data) < 0.5) return;

                                Event outputEvent = event;
                                if (binding.actions.setValue) {
                                    outputEvent.data = *binding.actions.setValue;
                                }
                                if (!binding.actions.modifyExprs.empty()) {
                                    detail::ModifyEvent(lock, outputEvent, event.data, binding, depth);
                                }
                                for (auto &dest : binding.outputs) {
                                    outputEvent.name = dest.queueName;
                                    SendEvent(lock, dest.target, outputEvent, depth + 1);
                                }
                            });
                            continue;
                        }
                    }
                    Event modifiedEvent = event;
                    if (binding.actions.setValue) {
                        modifiedEvent.data = *binding.actions.setValue;
                    }
                    if (!binding.actions.modifyExprs.empty()) {
                        if constexpr (LockType::template has_permissions<ReadSignalsLock>()) {
                            detail::ModifyEvent(lock, modifiedEvent, event.data, binding, depth);
                        } else {
                            ecs::QueueTransaction<ReadSignalsLock>([modifiedEvent, event, binding, depth](auto lock) {
                                Event outputEvent = modifiedEvent;
                                detail::ModifyEvent(lock, outputEvent, event.data, binding, depth);
                                for (auto &dest : binding.outputs) {
                                    outputEvent.name = dest.queueName;
                                    SendEvent(lock, dest.target, outputEvent, depth + 1);
                                }
                            });
                            continue;
                        }
                    }
                    for (auto &dest : binding.outputs) {
                        modifiedEvent.name = dest.queueName;
                        eventsSent += SendEvent(lock, dest.target, modifiedEvent, depth + 1);
                    }
                }
            }
        }
        return eventsSent;
    }
} // namespace ecs
