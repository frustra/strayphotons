#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalExpression.hh"

#include <algorithm>
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

        /**
         * Adds an event to any matching event input queues.
         *
         * A transactionId can be provided to synchronize event visibility with transactions.
         * If no transactionId is provided, this event will be immediately visible to all transactions.
         */
        size_t Add(const Event &event, size_t transactionId = 0) const;
        size_t Add(const AsyncEvent &event) const;
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
        std::optional<EventData> setValue;

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
        template<typename LockType>
        static size_t SendEvent(LockType lock, const EntityRef &target, const AsyncEvent &event, size_t depth = 0);

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
        bool FilterAndModifyEvent(DynamicLock<ReadSignalsLock> lock,
            sp::AsyncPtr<EventData> &output,
            const sp::AsyncPtr<EventData> &input,
            const EventBinding &binding);
    }

    template<typename LockType>
    size_t EventBindings::SendEvent(LockType lock, const EntityRef &target, const Event &event, size_t depth) {
        AsyncEvent asyncEvent = AsyncEvent(event.name, event.source, event.data);
        asyncEvent.transactionId = lock.GetTransactionId();
        return SendEvent(lock, target, asyncEvent, depth);
    }

    template<typename LockType>
    size_t EventBindings::SendEvent(LockType lock, const EntityRef &target, const AsyncEvent &event, size_t depth) {
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
                    AsyncEvent outputEvent = event;
                    if (!detail::FilterAndModifyEvent(lock, outputEvent.data, event.data, binding)) continue;

                    for (auto &dest : binding.outputs) {
                        outputEvent.name = dest.queueName;
                        eventsSent += SendEvent(lock, dest.target, outputEvent, depth + 1);
                    }
                }
            }
        }
        return eventsSent;
    }
} // namespace ecs
